# Your first game

This tutorial builds **Coin Chase**, a small game, from an empty folder to an
exported build: a square you move with the arrow keys picks up coins placed
in the editor, a score shows on screen, and a door leads to the next level.
It uses colored squares, so it needs no image. Every step can be played.

You need the editor and the runtime built (see
[Getting started](../getting-started.md)). The commands below assume they are
in `build/release/bin/`.

## 1. The game folder

A game is a folder. Create one anywhere, outside the engine's repository if
you like:

```
coin-chase/
├── game.json
├── scripts/
│   ├── main.lua
│   ├── player.lua
│   └── coin.lua
└── scenes/
    └── level1.json
```

`game.json` names the game and the scene it starts on:

```json
{
    "name": "Coin Chase",
    "start_scene": "level1"
}
```

The name is shown as the window title and gives the exported binary its name.

## 2. The scripts

Every `.lua` file under `scripts/` is loaded when the game starts. Each one
**declares** what it contains; `main.lua` holds the rules of the whole game.

### A coin

`scripts/coin.lua` declares the entity type `coin`, a **prefab**. Every field
written at the top of the file is the default of every coin:

```lua
entity "coin"

color  = { r = 255, g = 210, b = 0 }
scale  = 0.5
hitbox = { w = 32, h = 32 }
tags   = { "coin" }
value  = 1
```

- `color` with no `sprite` draws a 64 × 64 square of that color; `scale = 0.5`
  makes it 32 × 32.
- `hitbox` gives it a collider of that size, from its top-left corner.
- `tags` are labels other entities look for.
- `value` is a field the engine does not know: a **free field**. Because its
  default is a number, it is also a **script field**, which the editor lets
  you change per coin.

### The player

`scripts/player.lua`:

```lua
entity "player"

color         = { r = 80, g = 160, b = 255 }
hitbox        = { w = 64, h = 64 }
tags          = { "player" }
collides_with = { "coin" }
speed         = 400
score         = 0

local function axis(input, negative, positive)
    local value = 0
    if input:key(negative) then value = value - 1 end
    if input:key(positive) then value = value + 1 end
    return value
end

function update(self, dt)
    self.vx = axis(self.input, "Left", "Right") * self.speed
    self.vy = axis(self.input, "Up", "Down") * self.speed
end

function on_collide(self, other)
    self.score = self.score + other.value
    other:destroy()
    emit("scored", { total = self.score })
end
```

- `update(self, dt)` runs 60 times per second on the server. `self.input` is
  the keyboard of the player who owns this entity.
- Setting `vx` and `vy` (units per second) lets the engine move the entity in
  C++; there is nothing to multiply by `dt`.
- `collides_with = { "coin" }` asks the engine to call `on_collide` whenever
  this hitbox overlaps the hitbox of an entity tagged `coin`.
- `other:destroy()` removes the coin at the end of the tick.
- `emit` sends a game event to every client; the HUD below listens to it.

### The rules

`scripts/main.lua`:

```lua
event "scored" { total = "int" }

local score = 0

server = {
    on_start = function()
        print("Coin Chase started")
    end,

    on_player_joined = function(player)
        spawn("player", { x = 200, y = 200 + player.slot * 120, owner = player })
    end,
}

client = {
    on_start = function()
        on("scored", function(event)
            score = event.total
        end)
    end,

    draw_hud = function()
        draw.text("SCORE " .. score, 30, 28, 32)
    end,
}
```

- `event "scored"` declares an event and the kinds of its arguments.
- The game runs on a **server**; each window is a **client**. The `server`
  table holds the rules, the `client` table what each window does locally.
  In solo mode both run in the same process.
- `on_player_joined` spawns one player entity per person who connects, owned
  by them (`owner = player`), so its `self.input` is their keyboard. `slot` is
  0 for the first player, 1 for the second...
- `on("scored", ...)` keeps the last score received, and `draw_hud()` draws it
  in screen space every frame.

## 3. An empty scene

`scenes/level1.json` is the list of entities placed in the level. Start it
empty; the editor fills it:

```json
{
    "version": 1,
    "instances": []
}
```

The editor cannot create scene files yet, so the first one is written by hand.

## 4. Lay out the level

```sh
build/release/bin/editor coin-chase
```

The sidebar lists two **prefabs**, `coin` and `player`, and the scene
`level1`, already open because it is the start scene.

1. Drag `coin` from PREFABS into the viewport. The coin lands under the cursor,
   snapped to the 16 px grid. Place five or six of them.
2. The camera frame drawn in the viewport is what the game shows: keep the
   coins inside it.
3. Select one coin. In the inspector, the **Script** band shows `value 1`.
   Set it to `5`: the label turns to the accent color, meaning this coin
   **overrides** the prefab value.
4. In the **Sprite** band, set its color to `#FF5050` so the valuable coin
   stands out.
5. Save with `Ctrl+S`. The `•` next to the scene name disappears.

Open `scenes/level1.json`: each coin is one instance, and only the changed
coin has `overrides`:

```json
{
    "type": "coin",
    "position": [912, 304],
    "overrides": {
        "Script.value": 5,
        "Sprite.color": [255, 80, 80, 255]
    }
}
```

The player is not placed in the scene: it is spawned by `on_player_joined`,
once per player.

## 5. Play

Press **Play**. A game window opens with your level: a server and a client in
one process, starting from the scene as it is in the editor, saved or not.
Move with the arrow keys and pick up the coins; the score goes up by 5 on the
red one.

What the game prints shows in the editor's console. Close the window or press
**Stop** to come back to editing.

## 6. Change and play again

Nothing is compiled: the runtime reads the scripts each time it starts.

- In `player.lua`, set `speed = 700`, save the file, press Play: the player is
  faster.
- In `coin.lua`, set `value = 2`: every coin that does not override `value`
  now gives 2, the red one still gives 5.
- Make a mistake on purpose, such as `self.speeed`. Play: the console shows
  the error with its file and line, the faulty entity stops, the game and the
  editor keep running.

## 7. A second level

Add a door that loads the next scene. `scripts/door.lua`:

```lua
entity "door"

color      = { r = 40, g = 200, b = 90 }
hitbox     = { w = 64, h = 64 }
tags       = { "door" }
next_scene = "level2"
```

In `player.lua`, collide with doors too, keep the player across scenes, and
split the reaction by tag:

```lua
persistent    = true
collides_with = { "coin", "door" }

function collect(self, coin)
    self.score = self.score + coin.value
    coin:destroy()
    emit("scored", { total = self.score })
end

function on_collide(self, other)
    if other:has_tag("coin") then
        self:collect(other)
    elseif other:has_tag("door") then
        load_scene(other.next_scene)
    end
end
```

- `load_scene(name)` switches to `scenes/<name>.json` at the end of the tick:
  every entity is destroyed, then the scene's instances are spawned.
- `persistent = true` keeps the player entities, with their score, across
  the switch. Without it the players would disappear, since
  `on_player_joined` only runs when someone connects.
- `collect` is a **custom method**, called with a colon: `self:collect(other)`.
- Identify things by tag (`has_tag("coin")`), not by type name, so new kinds of
  coins work without touching the player.

Create `scenes/level2.json` like `level1.json`, restart the editor so it sees
the new prefab and scene, place a door in `level1`, then open `level2` from
SCENES and place more coins. `next_scene` is a string script field, so a
door can lead to any scene: set it in the inspector.

Play from `level1` and walk into the door.

## 8. Images and sounds

Assets live in `assets/`, and paths in scripts are relative to it.

- **A sprite:** put `coin.png` in `assets/sprites/` and write
  `sprite = "sprites/coin.png"` in `coin.lua`. The entity is drawn with the
  image (tinted by `color`, so remove `color` or set it to white) at the
  image's size times `scale`. Keep the hitbox matching the drawn size.
- **A sound:** put `coin.wav` in `assets/sounds/`, declare it once in
  `main.lua` with `sounds { "coin.wav" }`, and call `play("coin.wav", self)` in
  `collect`. Music goes in `assets/music/`, declared with `musics { ... }` and
  started with `music("name.mp3")`.

The editor's viewport shows sprites too. A missing file is drawn as the
tinted square and reported once; the game keeps running.

## 9. Play with friends

Every window is a client of a server, so multiplayer comes for free: each
person who joins gets their own player through `on_player_joined`. Try it
without the editor:

```sh
build/release/bin/engine-runtime --game coin-chase --host 4242        # you, hosting
build/release/bin/engine-runtime --game coin-chase 127.0.0.1 4242     # a second player
```

On a local network, the second player uses the host's IP address. Up to four
players can join. The HUD shows the last score any player sent; to show each
player their own score, add the player's `slot` to the event and filter on it. See [Run and export](run-and-export.md) for every mode.

## 10. Export

In the editor, click **Export**. The dialog proposes:

| Setting | Default |
|---|---|
| Name | `coin-chase`, from the `name` of `game.json` |
| Output folder | `coin-chase-export`, next to the game folder |

The dialog lists what will be copied, or what blocks the export. Click
**Export**, then:

```sh
cd coin-chase-export
./coin-chase --solo
```

The folder holds the runtime renamed `coin-chase`, the engine libraries, the
plugins and the game's files. Zip it and share it: it runs on a machine with
neither the editor nor the engine sources.

## Where next

- [Scripting a game](scripting.md): how to organise a bigger game.
- [Lua API reference](../reference/lua-api.md): cameras, HUD sprites, local
  effects, pathfinding, console variables.
- [R-Type example](../examples/r-type.md): a complete game, open it with
  `editor examples/r-type`.
