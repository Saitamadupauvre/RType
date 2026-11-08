# Scripting a game

How to organise the Lua of a game so it stays easy to change in the editor and
as it grows. Every function used here is in the
[Lua API reference](../reference/lua-api.md); [Your first game](first-game.md)
shows the basics step by step.

## The mental model

The engine is an ECS driven by Lua scripts.

| Concept | What it is | Where it lives |
|---|---|---|
| **Entity** | An id. No data, no logic. | The engine |
| **Component** | Engine data attached to an entity: position, sprite, collider, physics body, tags... | C++, reached through native fields |
| **Prefab** | An entity type declared with `entity "name"`: default fields, callbacks, methods | `scripts/**/*.lua` |
| **Instance** | One entity of a prefab placed in a scene, with its overrides | `scenes/*.json`, edited in the editor |
| **Main script** | Rules of the whole game: players, events, HUD, scene switches | `scripts/main.lua` |

**Native fields** (`x`, `y`, `vx`, `vy`, `sprite`, `color`, `tags`, `hitbox`,
`collides_with`, `scale`, `rotation`, `z_index`...) are not stored in Lua:
reading or writing them goes to the engine's components, which move, collide,
draw and replicate the entity. Every other field (`speed`, `health`...) is a
**free field** in the entity's Lua table. A free field whose default is a
number, boolean, string, `{ x, y }`, color or list is a **script field**: the
editor shows it and scenes can override it per instance.

## Server and client

A game always runs as one **server** and one or more **clients**, even alone
(solo mode puts both in one process). Both load the same scripts:

| Side | Runs | Typical code |
|---|---|---|
| Server | Entity callbacks (`on_spawn`, `update`, `on_collide`, `on_destroy`), `server = { ... }` of `main.lua` | Movement, rules, spawning, scores, scene switches |
| Client | `client = { ... }` sections of declarations and of `main.lua` | HUD, local effects, local hotkeys (mute), camera overrides |

The server is authoritative: clients only send their keyboard and mouse, and
draw what the server replicates. Top-level code of a file runs on both sides,
so it may only use the shared (`core`) functions; side-specific calls go in
callbacks or sections. Calling a function the side does not have is a plain
Lua error (`attempt to call a nil value (global 'spawn')`). The full table is
in the [Lua API](../reference/lua-api.md#networking-from-the-scripts-point-of-view).

## One server tick

Sixty times per second:

```
1. inputs          → self.input of the entities each player owns
2. entity update   → update(self, dt) of every live entity, then update(dt) of main.lua
3. navigation      → entities following a path, on_arrive
4. movement        → vx / vy applied by the engine
5. collisions      → hitboxes that overlap, on_collide(self, other)
6. destruction     → on_destroy(self) of entities marked with destroy(), then removal
7. scene switch    → load_scene, if called this tick
8. snapshot        → every entity with a sprite or a color is sent to clients
```

Gameplay runs at a fixed step, never per frame. Multiply by `dt` whatever you
change by hand in `update`, or set `vx` / `vy` and let the engine move the
entity.

## Where does this logic go?

Ask in order:

1. **Does it concern a single entity type?** → That prefab's file:
   `update`, `on_collide`, custom methods. Examples: how a ship moves, how an
   enemy flies.
2. **Is it a value a level designer may want to change per instance?** → A
   script field with a sensible default (`health = 3`, `next_scene = "level2"`),
   changed in the editor's inspector.
3. **Is it the layout of a level?** → A scene, edited in the editor, not
   `spawn` calls in `on_start`.
4. **Does it span several entity types?** → A coordinating entity (see
   [What is not there yet](#what-is-not-there-yet)) or `main.lua`. Examples:
   spawning waves, rounds, game over.
5. **Is it only for one player's screen?** → The `client` section of
   `main.lua`: HUD, sounds toggles, local effects.
6. **Would it make sense in any game, and Lua cannot do it?** → It belongs in
   the engine: open an issue (see [Contributing](../contributing.md)).

## Organising files

```
scripts/
├── main.lua              rules: server and client sections
├── player/ship.lua       one prefab per file (short form)
├── enemies/...
└── lib/                  shared code, loaded only with require
    ├── config.lua        every tunable number
    └── controls.lua      key bindings
```

- Every `.lua` file under `scripts/` is loaded, in any folder, except `lib/`.
- A file holds **one short form** (`entity "name"` and the rest of the file is
  the prefab) or **any number of table forms** (`entity "name" { ... }`).
- Names are unique across the game; a duplicate fails the load and names both
  files.
- `require("config")` loads `lib/config.lua` once and returns its value;
  `require("enemies.wave")` loads `lib/enemies/wave.lua`.

### Keep numbers in one place

Put tunables in a `lib/config.lua` table and copy them into fields at the top
of each prefab, so they can still be overridden at spawn time or in a scene:

```lua
-- lib/config.lua
return {
    enemy = { speed = 300, hitbox = { w = 64, h = 64 } },
}
```

```lua
-- enemies/enemy.lua
local config = require("config")

entity "enemy"

speed  = config.enemy.speed
hitbox = config.enemy.hitbox
```

```lua
spawn("enemy", { x = 1920, y = 300, speed = 450 })   -- this one is faster
```

### Read input through bindings

`self.input` holds raw keys and mouse buttons (`input:key("Space")`,
`input:mouse("Left")`, `input:pointer()`). Map them to actions in one
`lib/controls.lua`, so remapping a key is a one-line change:

```lua
-- lib/controls.lua
local actions = { fire = { "Space", "Enter" } }
local axes = { move_x = { negative = { "Left", "Q" }, positive = { "Right", "D" } } }

local controls = {}

function controls.pressed(input, action)
    for _, key in ipairs(actions[action]) do
        if input:key(key) then return true end
    end
    return false
end

function controls.axis(input, axis)
    local value = 0
    for _, key in ipairs(axes[axis].negative) do
        if input:key(key) then value = value - 1; break end
    end
    for _, key in ipairs(axes[axis].positive) do
        if input:key(key) then value = value + 1; break end
    end
    return value
end

return controls
```

## Rules

### Identify by tags, not by type names

`other:has_tag("enemy")` keeps working when new enemy types are added;
`other.type == "enemy"` does not. Collisions use tags too:
`collides_with = { "enemy" }`.

### Destroy, then react in `on_destroy`

`self:destroy()` only marks the entity: it is removed at the end of the tick,
after `on_destroy(self)`. Put side effects (score, explosion, sound) in
`on_destroy` so every way of dying triggers them. A destroyed entity gets no
further `on_collide` in the same tick.

### Use `self:method()`

Methods are called with a colon: `self:shoot(dt)`, never `self.shoot(dt)`.

### Keep per-instance data out of shared tables

Free fields holding a table are copied for each entity at spawn. Data meant to
be shared by every instance, such as a module from `require`, belongs in a
`local` of the file. Name a field `_cache` (leading underscore) to keep it out
of the editor.

### Persistent entities across scenes

`load_scene(name)` destroys every entity whose prefab does not set
`persistent = true`. Mark the player entities and the game controller
persistent, spawn them from scripts, and let scenes hold the level.

## What is not there yet

Some functions of the [Lua API](../reference/lua-api.md) are reserved but not
implemented yet (`find_all`, `count`, `after`, `every`, `wait`, `behavior`,
`random`, running `system` declarations). Until they land, use these
stand-ins:

| Missing | Stand-in | Once it lands |
|---|---|---|
| `find_all`, `count` | A `lib/roster.lua` of groups: entities join in `on_spawn`, leave in `on_destroy` | `find_all("enemy")` |
| Running `system "name"` | An entity without sprite or color (it is not drawn or replicated) whose `update` runs the logic | `system "waves"` |
| `after`, `every` | A time counter in `update` | `every(1.5, ...)` |
| `random` | `math.random` | `random(1, 10)` |
| `behavior`, `wait` | A state field and counters in `update` | Sequences written top to bottom |

The [R-Type example](../examples/r-type.md) uses every stand-in: `lib/roster.lua`,
the `game` and `bydo_spawner` controller entities, and counters.

## Tests for games

Scripts can be checked without a window: run the game as a dedicated server
and read the console.

```sh
engine-runtime --game my-game --server 4242
```

A syntax error, a duplicate name or a bad scene is logged at startup with its
file and line, before any player joins.

## Anti-patterns

| Don't | Do |
|---|---|
| `if other.type == "enemy"` | `if other:has_tag("enemy")` |
| `spawn` the level's walls in `on_start` | Place them in a scene with the editor |
| Magic numbers in prefab files | A value in `lib/config.lua`, or a script field |
| Checking raw keys everywhere | `lib/controls.lua` actions |
| Logic depending on frame rate | Multiply by `dt`, or set `vx` / `vy` |
| Side effects next to every `destroy()` | One `on_destroy` |
| `self.method()` | `self:method()` |
