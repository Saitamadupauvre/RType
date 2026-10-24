# Writing the game

How R-Type gameplay is coded on top of the engine, with tutorials that work
with the engine as it is today. The full Lua API, including what is planned,
is in [PROJECT.md §10](PROJECT.md#10-lua-scripting-api).

## The mental model

The engine is an ECS driven by Lua scripts.

| Concept | What it is | Where it lives |
|---|---|---|
| **Entity** | An id. No data, no logic. | `engine::core::EntityManager` |
| **Component** | Plain data attached to an id: `Position`, `Velocity`, `Sprite`, `Tags`... | C++ component pools |
| **Native system** | C++ loop over every entity with given components | `engine/` |
| **Entity script** | Lua callbacks of one entity type (`update`, `on_hit`...) | `game/scripts/` |
| **Lua system** | Lua logic spanning several entity types (waves, score) | `game/scripts/` |

Native fields (`self.x`, `self.y`, `self.vx`, `self.vy`, `self.sprite`,
`self.color`, `self.tags`) are not stored in Lua: reading or writing them goes
straight to the C++ components. Every other field (`speed`, `health`,
`hitbox`...) is a free field stored in the entity's Lua table.

All scripts run **on the server only**. The client draws the replicated
entities and never runs Lua.

## One tick

What the server does every 1/60 s today:

```
1. inputs          → self.input of the entities each player owns
2. entity update   → update(self, dt) of every live entity
3. destruction     → on_destroy(self) of entities marked with destroy(), then removal
4. snapshot        → every entity with a sprite or a colour is sent to clients
```

Always multiply by `dt`: gameplay runs at a fixed step, never per frame.

## Where does this logic go?

Ask in order:

1. **Would it make sense in a platformer or any other game?**
   → C++ in `engine/`, exposed to Lua. Examples: movement, collisions, sound,
   timers.
2. **Does it concern a single entity type?**
   → That entity's file (`update`, `on_hit`, custom methods).
   Examples: how the ship moves, how a Bydo flies.
3. **Does it span several entity types?**
   → A coordinating entity today, a Lua `system` once systems run (see
   [Current state](#current-state)). Examples: spawning waves, collisions,
   rounds.

Never put gameplay in `game/client` or `game/server` C++: those files only wire
engine modules together.

## Tutorial: add a new enemy

Goal: a "zigzag" Bydo that flies left along a sine wave and takes two hits.

**1. Add its sprite.** The client draws an entity with
`assets/sprites/<type>.png`, so name the file after the type:
`assets/sprites/zigzag.png`. Without the file, the client draws the
placeholder.

**2. Add its tunables** to `game/scripts/lib/config.lua`:

```lua
zigzag = { hitbox = { w = 64, h = 64 }, speed = 250, amplitude = 120, frequency = 3, health = 2 },
```

**3. Declare it** in `game/scripts/enemies/zigzag.lua`:

```lua
local config = require("config")
local roster = require("roster")

entity "zigzag"

tags   = { "enemy" }
sprite = "sprites/zigzag.png"
speed  = config.zigzag.speed
hitbox = config.zigzag.hitbox
health = config.zigzag.health

function on_spawn(self)
    self.base_y = self.y
    roster.add(self, "enemies")
end

function on_destroy(self)
    roster.remove(self)
end

function on_hit(self, other)
    if not other:has_tag("missile") then
        return
    end
    self.health = self.health - 1
    if self.health <= 0 then
        roster.destroy(self)
    end
end

function update(self, dt)
    local zigzag = config.zigzag
    self.x = self.x - self.speed * dt
    self.y = self.base_y + math.sin(self.age * zigzag.frequency) * zigzag.amplitude
    if self.x <= -self.hitbox.w then
        roster.destroy(self)
    end
end
```

Joining the `enemies` group is all it takes for `systems/collisions.lua` to
test it against missiles and ships.

**4. Spawn it.** In `enemies/spawner.lua`, spawn a zigzag instead of a Bydo
from time to time:

```lua
local kind = math.random() < 0.25 and "zigzag" or "bydo"
spawn(kind, { x = config.world.width, y = math.random(0, config.world.height - config.bydo.hitbox.h) })
```

**5. Run it.** Rebuild (the build copies `scripts/` and `assets/` next to the
executables) and restart the server. No C++ change, no new network message:
the new type is added to the type table sent at connection.

**6. Keep the test fixture in sync** (see [Tests](#tests)).

## Tutorial: tune the game

Every number is in `game/scripts/lib/config.lua`:

| Want | Change |
|---|---|
| Faster ships | `ship.speed` |
| Shoot more often | `ship.fire_cooldown` (seconds between missiles) |
| More enemies | `bydo.spawn_interval` (seconds between spawns) |
| Faster enemies | `bydo.speed` |
| Longer pause after game over | `game.restart_delay` |
| Other player colours | `ship.colors` (one per slot) |

## Rules

### Identify by tags, not by type names

`other:has_tag("enemy")` keeps working when new enemy types are added;
`other.type == "bydo"` does not.

### Destroy through the roster

Use `roster.destroy(entity)`: it removes the entity from its groups at once,
so it is never hit twice in the same tick, then calls `entity:destroy()`.
Destruction itself is deferred to the end of the tick; side effects go in
`on_destroy`.

### Read input as actions

Use `self.input:axis("move_x")`, `self.input:axis("move_y")` and
`self.input:pressed("fire")`. Never refer to keys: bindings belong to the
client.

### Keep data in config and fields

Tunable values come from `lib/config.lua` and are copied into free fields at
the top of the entity file, so they can be overridden at spawn time:

```lua
spawn("bydo", { x = 1920, y = 300, speed = 450 })
```

### Use `self:method()`

Methods are called with a colon: `self:shoot(input, dt)`, never
`self.shoot(...)`.

## Current state

Some engine features of the target design are not wired into the server tick
yet. Until they land, the game uses Lua stand-ins. Each one goes away once its
engine counterpart exists:

| Missing in the engine | Stand-in today | Once it lands |
|---|---|---|
| Native movement in the server tick | `update` writes `self.x` / `self.y` | Set `vx` / `vy` and let C++ integrate |
| Native collisions (`engine-physics` exists but is not in the tick) | `systems/collisions.lua` tests every pair with `lib/aabb.lua` and calls `on_hit` on both | Declare `hitbox`, react in `on_hit`, delete both files |
| `find_all`, `count` | `lib/roster.lua` groups, joined in `on_spawn`, left in `on_destroy` | `find_all("enemy")` |
| Running Lua `system` declarations | Entities without a sprite: `game`, `bydo_spawner`, `collisions` | `system "waves"` |
| `after`, `every`, `random` | Time counters in `update`, `math.random` | `every(1.5, ...)`, `random(...)` |
| `play`, `music` | No sound | Issues #118, #119 |
| `behavior` coroutines, `wait` | Counters in `update` | Sequences written top to bottom |

The target style (velocity, native collisions, systems, coroutines) is
described in [PROJECT.md §10](PROJECT.md#10-lua-scripting-api).

## Tests

Tests load a copy of the scripts from `tests/fixtures/game/server/game/`,
identical except for an empty `on_start` in `main.lua`, so each test spawns
only what it checks. Re-sync it after editing a script:

```sh
rsync -a --delete --exclude /main.lua game/scripts/ tests/fixtures/game/server/game/
```

Then add a test in `tests/game/server/` that spawns your entity and checks its
behaviour, like `BydoTest.cpp` or `ShipTest.cpp`.

## Anti-patterns

| Don't | Do |
|---|---|
| `if other.type == "bydo"` | `if other:has_tag("enemy")` |
| `entity:destroy()` on something in a roster group | `roster.destroy(entity)` |
| Gameplay code in `game/client/` or `game/server/` | A Lua entity |
| Checking keys in Lua | `self.input` actions |
| Magic numbers in entity files | A value in `lib/config.lua` |
| Logic depending on frame rate | Multiply by `dt` |
