# Writing the game

How R-Type gameplay should be coded on top of the engine. The full Lua API is in
[PROJECT.md §10](PROJECT.md#10-lua-scripting-api); this page explains **where
each piece of logic goes and why**.

## The mental model

The engine is an ECS driven by Lua scripts.

| Concept | What it is | Where it lives |
|---|---|---|
| **Entity** | An id. No data, no logic. | `engine::core::EntityManager` |
| **Component** | Plain data attached to an id: `Position`, `Velocity`, `Sprite`, `Tags`... | C++ component pools |
| **Native system** | C++ loop over every entity with given components: movement, collisions, rendering, replication | `engine/` |
| **Entity script** | Lua callbacks of one entity type (`update`, `on_hit`...) | `game/scripts/` |
| **Lua system** | Lua logic spanning several entity types (waves, score) | `game/scripts/` |

An entity's Lua `update(self, dt)` is **not** a system. It is the behaviour of
one entity type, like a Unity `MonoBehaviour`. The system is the C++ loop that
calls it (`ScriptRuntime::update`).

Native fields (`self.x`, `self.vx`, `self.sprite`, `self.color`, `self.tags`)
are not stored in Lua: reading or writing them goes straight to the C++
components. Lua **describes intent by editing components**; native systems do
the heavy work.

## One tick

```
1. inputs        → self.input of player entities
2. Lua systems   → system "waves", "score"...
3. entity update → update(self, dt), behavior coroutines, after/every timers
4. movement      → C++: position += velocity * dt
5. collisions    → C++: overlap test, then on_hit(self, other)
6. destruction   → on_destroy(self), entity removed
7. snapshot      → sent to clients (server)
8. render        → C++: every Position + Sprite (client)
```

Gameplay runs at a fixed 60 Hz step, never per frame: always multiply by `dt`.

## Where does this logic go?

Ask in order:

1. **Would it make sense in a platformer or any other game?**
   → C++ native system in `engine/`, exposed to Lua through a field or function.
   Examples: movement, collisions, rendering, sound, timers.
2. **Does it concern a single entity type?**
   → That entity's file (`update`, `on_hit`, custom methods).
   Examples: how the player moves, how a Bydo zigzags, how the boss attacks.
3. **Does it span several entity types?**
   → A Lua `system`.
   Examples: spawning waves, score, respawning the player.

Never put gameplay in `game/client` or `game/server` C++: those files only wire
engine modules together.

## Rules

### Move with velocity, not position

```lua
function update(self, dt)
    self.vx = self.input:axis("move_x") * self.speed
    self.vy = self.input:axis("move_y") * self.speed
end
```

Set `vx` / `vy` and let the native movement system integrate them. Write
`self.x` / `self.y` directly only for teleports or non-linear paths (sine waves).

### Let the engine detect collisions

Give the entity a `hitbox` and `tags`, react in `on_hit`. Never loop over other
entities in Lua to test overlaps.

```lua
entity "bydo"

sprite = "bydo.png"
hitbox = { w = 32, h = 32 }
tags   = { "enemy" }
health = 1

function on_hit(self, other)
    if other:has_tag("player_missile") then
        self:damage(1)
    end
end
```

### Identify by tags, not by type names

`other:has_tag("enemy")` keeps working when new enemy types are added;
`other.type == "bydo"` does not.

### Destroy with `self:destroy()`

Destruction is deferred to the end of the tick. Cleanup and side effects
(explosions, score, drops) go in `on_destroy` or in a Lua system listening to
`on_destroy`.

### Read input as actions

Use `self.input:axis("move_x")` and `self.input:pressed("fire")`. Never refer
to keys: bindings belong to the client.

### Keep data in fields

Tunable values (`speed`, `health`, `points`, `fire_rate`) are free fields at
the top of the file, overridable at spawn time:

```lua
spawn("bydo", { x = world.width + 50, y = 300, speed = 250 })
```

### Use coroutines for sequences

Multi-step patterns (boss phases, scripted movements) go in `behavior` with
`wait()`, instead of state machines with timers in `update`.

### Server owns the logic

All callbacks run on the server only. The client only draws replicated
entities. Mark a free field `synced(...)` only when clients must display it.

## File layout

```
game/scripts/
├── main.lua              on_start(): spawn the first entities
├── player/
│   ├── player.lua        entity "player"
│   └── missile.lua       entity "player_missile"
├── enemies/
│   ├── bydo.lua
│   └── boss.lua
├── stages/
│   └── stage1.lua        system "stage1_waves"
├── systems/
│   └── score.lua         system "score"
└── lib/                  helpers reached with require()
```

One declaration per file by default. Small related declarations (a weapon and
its projectile) may share a file with the table form.

## Example: player that shoots

```lua
entity "player"

sprite    = "ship.png"
hitbox    = { w = 32, h = 16 }
tags      = { "player" }
speed     = 400
fire_rate = 0.2
cooldown  = 0

function update(self, dt)
    self.vx = self.input:axis("move_x") * self.speed
    self.vy = self.input:axis("move_y") * self.speed

    self.cooldown = self.cooldown - dt
    if self.input:pressed("fire") and self.cooldown <= 0 then
        self.cooldown = self.fire_rate
        spawn("player_missile", { x = self.x + 32, y = self.y })
    end
end

function on_hit(self, other)
    if other:has_tag("enemy") then
        self:destroy()
    end
end
```

```lua
entity "player_missile"

sprite = "missile.png"
hitbox = { w = 16, h = 4 }
tags   = { "player_missile" }
vx     = 900

function update(self, dt)
    if self.x > world.width then
        self:destroy()
    end
end

function on_hit(self, other)
    if other:has_tag("enemy") then
        self:destroy()
    end
end
```

## Anti-patterns

| Don't | Do |
|---|---|
| `self.x = self.x + speed * dt` | `self.vx = speed` |
| AABB test against `find_all(...)` in `update` | `hitbox` + `on_hit` |
| `if other.type == "bydo"` | `if other:has_tag("enemy")` |
| Gameplay code in `game/client/main.cpp` | Lua entity or system |
| Checking keys in Lua | `self.input` actions |
| Timer counters for scripted sequences | `behavior` + `wait()` |
| Logic depending on frame rate | Use `dt` from the fixed step |

## Current state

Not every native system exists yet. Until they land, the multiplayer game in
`game/scripts/` uses Lua stand-ins. Each one goes away once its engine
counterpart is wired into the tick:

| Missing in the engine | Stand-in | Once it lands |
|---|---|---|
| Native movement in the server tick (#76) | `update` writes `self.x` / `self.y` | Set `vx` / `vy` instead |
| Native collisions | `systems/collisions.lua` tests every `hitbox` pair with `lib/aabb.lua` and calls `on_hit` on both entities | Delete both files: entities already declare `hitbox` and react in `on_hit` |
| `find_all` | `lib/roster.lua`: entities join a group in `on_spawn` and leave it in `on_destroy`; `roster.destroy(e)` leaves at once, so a doomed entity is never hit twice in one tick | Delete it and use `find_all` with tags |
| Lua `system` declarations | Entities without a sprite: `game`, `bydo_spawner`, `collisions` | Declare them as `system "..."` |
| `every`, `random` | Time counters in `update`, `math.random` | `every(interval, ...)`, `random(...)` |

The game runs on the server. `main.lua` spawns the `game` entity and forwards
`on_player_joined` / `on_player_left` to it: `game` spawns a ship owned by each
player, so each ship reads the input of its own player, and restarts on a timer
after a game over. The client draws an entity with `sprites/<type>.png`, so
the sprite of a replicated entity type must be named after the type.

Tests load a copy of the scripts from `tests/fixtures/game/server/game/`,
identical except for an empty `on_start` in `main.lua`, so each test spawns only
what it checks. Re-sync it after editing a script:

```sh
rsync -a --delete --exclude /main.lua game/scripts/ tests/fixtures/game/server/game/
```
