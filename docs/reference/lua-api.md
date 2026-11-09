# Lua API reference

Every game made with the engine is written in Lua: entities, rules, input,
sound, HUD. This page is the complete reference of what scripts can call. For
a guided introduction, start with [Your first game](../guide/first-game.md)
and [Scripting a game](../guide/scripting.md).

This page is the specification of the Lua API. Not everything is implemented yet:

| Feature | Status |
|---|---|
| `entity "name"` declarations (short and table forms), `require` from `lib/`, sandbox | Implemented |
| `system "name"` declarations | Parsed and validated; systems do not run yet |
| `spawn(type, overrides)`, `players()`, `find_player(id)` | Implemented |
| `main.lua`: `on_start`, `on_player_joined`, `on_player_left`, `on_scene_loaded`, `update(dt)`, `server` / `client` sections | Implemented |
| `load_scene(name)`, `persistent = true` entities, built-in `scene_loaded` event | Implemented (see [Global functions](#global-functions)) |
| Entity fields `x`, `y`, `vx`, `vy`, `sprite`, `color`, `tags`, `hitbox`, `collides_with`, `id`, `type`, `age`, `input`, `owner`; free fields | Implemented |
| Entity methods `destroy()`, `has_tag()`, `get_field()`, `set_field()`; callbacks `on_spawn`, `update`, `on_collide`, `on_destroy` | Implemented |
| Native collisions: `engine-physics` detects `hitbox` overlaps in the server tick and calls `on_collide` (see [Entity API (`self`)](#entity-api-self)) | Implemented |
| Native movement from `vx` / `vy` in the server tick (`engine-physics` integrates the body velocity) | Implemented |
| `sounds { ... }`, `musics { ... }`, `play(name, entity)`, `music(name)` | Implemented (see [Global functions](#global-functions)) |
| `textures { ... }`, entity `sprite` replicated to clients by texture id | Implemented (see [Global functions](#global-functions)) |
| `shaders { ... }`, entity `shader` replicated to clients by shader id and drawn through it | Implemented (see [Entity API (`self`)](#entity-api-self), [Global functions](#global-functions)) |
| API sets (`core`, `server`, `client`, `editor`), `server = { ... }` / `client = { ... }` sections, client `on(name, handler)` | Implemented (see [Networking from the script's point of view](#networking-from-the-scripts-point-of-view)) |
| Client `input` (`key`, `mouse`, `pointer`, `pressed`) and `audio` (`muted`, `set_muted`, `volume`, `set_volume`) | Implemented (see [Networking from the script's point of view](#networking-from-the-scripts-point-of-view)) |
| Client `spawn_local` (local entities) and `draw.text`, `draw.rect`, `draw.sprite` from `draw_hud()` (screen-space HUD) | Implemented (see [Networking from the script's point of view](#networking-from-the-scripts-point-of-view)) |
| Entity `z_index` (drawing order), replicated | Implemented (see [Entity API (`self`)](#entity-api-self)) |
| Pathfinding: `navgrid`, `find_path`, `self:navigate_to`, `stop_navigation`, `is_navigating`, `on_arrive` (see [Global functions](#global-functions)) | Implemented |
| `find_all`, `find_first`, `count`, `after`, `every`, `wait`, `behavior`, `random`, `world`, `log`, `synced`, `add_tag`, `remove_tag`, `distance_to`, `move_to` | Planned (names already reserved) |

[Scripting a game](../guide/scripting.md#what-is-not-there-yet) shows how to work around what is missing.

## Principles

- The whole game is written in Lua. The developer never touches C++.
- **Files are free to live anywhere** in the game folder. The engine loads every `.lua` file recursively.
- **Each file declares what it contains**: `entity "name"` or `system "name"`. A file usually declares one thing; small or related things can share a file using the table form.
- A file uses **either** one short form (`entity "name"` alone, the whole file is the prefab) **or** any number of table forms (`entity "name" { ... }`). Two short forms in one file, or a short form mixed with table forms, is a load-time error and none of the file's declarations are registered.
- A file outside `lib/` that declares nothing (other than `main.lua`) is loaded but logs a warning.
- Names are **unique**. Two declarations with the same name are a load-time error that shows both file paths.
- `main.lua` at the root is the entry point.
- Utility code used with `require` lives in `lib/` and is **not** loaded automatically.
- `require("name")` loads `lib/name.lua`; dots are folders (`require("enemies.wave")` loads `lib/enemies/wave.lua`). Each module runs once, in its own environment, and its return value is cached (`true` if it returns nothing). Names only contain letters, digits, `_` and `.`, so a script cannot leave `lib/`. Modules cannot declare entities or systems, circular requires are an error, and precompiled bytecode is rejected. The standard `package` library is not available.

## Loading order

1. Load every declaration file (entities and systems are registered, nothing is spawned).
2. On the server, spawn the `start_scene` named by `game.json`, if any: each instance runs its `on_spawn`, then gets its overrides (see [Game folder and scenes](../guide/game-folder.md)).
3. Merge the `server = { ... }` or `client = { ... }` table of `main.lua` into it (see [Networking from the script's point of view](#networking-from-the-scripts-point-of-view)), then call `on_start()` from `main.lua`, which already sees the scene's entities. `on_player_joined(player)` and `on_player_left(player)` are called later, whenever a player connects or leaves; `update(dt)`, when `main.lua` defines one, runs once per tick after every entity's `update`.

Since `spawn` is only used once the game has started, file order never matters.

## Declaring an entity

**One entity per file** (default style):

```lua
-- stage1/enemy.lua
entity "enemy"

-- native fields: handled by the C++ engine
sprite        = "enemy.png"
hitbox        = { w = 32, h = 32 }
tags          = { "enemy" }
collides_with = { "player_missile" }

-- free fields: invented by the developer
health = 1
speed  = 150
points = 100

-- callbacks: called by the engine
function on_spawn(self)
    self.base_y = self.y
end

function update(self, dt)
    self.x = self.x - self.speed * dt
    self.y = self.base_y + math.sin(self.age * 3) * 40
    if self.x < -50 then self:destroy() end
end

function on_collide(self, other)
    self:damage(1)
end

-- custom methods: as many as needed
function damage(self, amount)
    self.health = self.health - amount
    if self.health <= 0 then self:explode() end
end

function explode(self)
    spawn("explosion", { x = self.x, y = self.y })
    play("explosion.wav", self)
    self:destroy()
end
```

**Several declarations in one file** (table form):

```lua
-- player/player.lua
entity "player" {
    sprite = "ship.png",
    speed  = 400,

    update = function(self, dt)
        -- ...
    end,
}

entity "player_missile" {
    sprite = "missile.png",
    vx     = 900,
    tags   = { "player_missile" },
}

system "respawn" {
    on_destroy = function(e)
        if e.type == "player" then
            after(3, function() spawn("player") end)
        end
    end,
}
```

Both forms produce the same internal representation.

`entity "x" { ... }` is plain Lua sugar for `entity("x")({ ... })`: `entity "x"` returns a declarator, and calling it with a table gives the declaration its body. A declarator that is never called is a short form, and the file's environment becomes the prefab. This is why a file holds **at most one** short form and never mixes it with table forms: every name the file defines would otherwise belong to two declarations at once.

## What a file contains

| Kind | Examples | Stored in | Used by |
|---|---|---|---|
| Native fields | `x`, `y`, `vx`, `vy`, `sprite`, `color`, `z_index`, `shader`, `rotation`, `scale`, `hitbox`, `collides_with`, `tags`, `zoom`, `follow`, `bounds` | C++ component pools | Movement, collisions, rendering, replication |
| Free fields | `health`, `speed`, `points`, `waypoints` | Lua table of the entity | Scripts, and scenes / the editor as script fields |
| Callbacks | `on_spawn`, `update`, `on_collide`, `on_destroy`, `behavior` | Prefab table | Called by the engine |
| Custom methods | `damage`, `explode` | Prefab table (shared by all instances) | Called with `self:name()` |

Methods are not copied into each instance: they stay in the prefab table, so reloading a file updates every live entity.

Free fields holding a table are the exception: each entity gets its own deep copy at spawn, so `self.waypoints[1] = nil` changes only that entity, never the prefab or the other instances. Nested tables and cycles are copied too (at most 64 levels deep, or the spawn fails); functions and metatables stay shared. Data meant to be shared by every instance, such as a module from `require`, belongs in a `local` of the file, not in a prefab field.

A free field whose default is a number, boolean, string, `{ x, y }`, `{ r, g, b[, a] }` or a non-empty array of one of these is a **script field**: scenes and the editor store a per-instance value for it under `"Script.<name>"` (an integer default makes an int field, `3.0` a float one). Names starting with `_`, functions, other tables and fields declared only in a `server`/`client` section are never script fields; name a field `_cache` to keep it out of the editor. See [Game folder and scenes](../guide/game-folder.md#script-fields).

## Entity API (`self`)

```lua
self.x, self.y      -- position (native)
self.vx, self.vy    -- velocity: moved by the C++ engine every tick, nil until set
self.rotation       -- degrees, clockwise (native), nil until set
self.scale          -- { x, y } factor (native), nil until set
self.age            -- seconds since spawn (read-only)
self.id             -- unique id (read-only)
self.type           -- declared name, e.g. "enemy" (read-only)
self.owner          -- player controlling the entity, or nil

self:destroy()                -- deferred to the end of the tick
self:has_tag("enemy")
self:get_field("PhysicsBody", "mass")      -- any registered component field, nil if absent
self:set_field("PhysicsBody", "mass", 2.5) -- adds the component if needed
self:add_tag("phase2")
self:remove_tag("phase2")
self:distance_to(other)
self:move_to(x, y, duration)  -- inside a coroutine

self:navigate_to(target, 200) -- follow a path to a point or an entity at 200 units/s
self:stop_navigation()
self:is_navigating()
function on_arrive(self) end  -- called once the target is reached
```

`sprite` is a path relative to `assets/`, such as `sprites/boss/core.png`. The `sprite` of an entity declaration is declared as a texture on its own; any other value must be declared with `textures { ... }` ([Global functions](#global-functions)) or be `""` (no sprite), otherwise the assignment raises an error and the sprite does not change. `tags` reads back as a new array: change tags by assigning a whole array (`self.tags = { "enemy" }`); entries must be strings and duplicates are dropped. `color` is a table `{ r, g, b, a }` of numbers in [0, 255] (`a` defaults to 255, the default color is white): it tints the sprite, and an entity with a color but no `sprite` is drawn as a 64 × 64 square of that color (before `scale`). It reads back as a new table.

`shader` is the name of a GLSL program under `assets/`, without extension, such as `shaders/space`: the client draws the entity (its sprite or its square) through `assets/shaders/space.fs` and, when it exists, `assets/shaders/space.vs`. Like `sprite`, the `shader` of an entity declaration is declared on its own; any other value must be declared with `shaders { ... }` ([Global functions](#global-functions)) or be `""` (no shader). It is replicated by shader id and the client preloads every declared shader. The engine sets two uniforms before drawing: `float time`, seconds since the client started (from the engine clock, never the frame rate), and `vec2 resolution`, the virtual size of the screen (1920 × 1080), so `gl_FragCoord.xy / resolution` spans the screen. A shader that is missing or does not compile is logged once and the entity is drawn without it. Entities are drawn in ascending `z_index`, then in spawn order, so a background either gets a low `z_index` or is spawned first:

```lua
entity "space"

shader  = "shaders/space"
z_index = -100
color   = { r = 0, g = 0, b = 0 }
scale  = { x = 1920 / 64, y = 1080 / 64 }   -- the 64 x 64 square covers the screen
```

`rotation` turns the sprite, in degrees clockwise, around its center. `scale` takes a number for both axes (`scale = 2`) or a table `{ x = .., y = .. }`; a negative factor mirrors the sprite (`scale = { x = -1, y = 1 }` faces it the other way) and `0` hides it. The scaled sprite keeps its top-left corner at `x`, `y`. Both are optional: an entity without them is drawn upright at the size of its texture and sends nothing over the network for them. `scale` always reads back as a new `{ x, y }` table. Both are replicated and editable, so the editor, scene files and snapshots carry them; the scale travels as two floats, so fractional factors reach clients exactly. Colliders do not follow them yet: `hitbox` stays exactly as written, whatever the rotation or scale. An entity drawn as a colored square (no `sprite`) is scaled but never rotated.

`z_index` is a whole number from -2^31 to 2^31 - 1 that orders drawing: lower values are drawn first, so higher ones appear in front; entities with the same `z_index` are drawn in spawn order. It is optional: an entity without it reads `nil`, is drawn at 0 and sends nothing over the network for it. Replicated entities and the client's local entities ([Networking from the script's point of view](#networking-from-the-scripts-point-of-view)) are sorted together. A value that is not a whole number, or that does not fit, raises an error and keeps the old one.

```lua
entity "boss" {
    sprite   = "sprites/boss.png",
    scale    = 3,
    rotation = 15,
}
```

`hitbox = { w, h }` gives the entity a rectangular collider of positive size, anchored at its top-left `x`, `y` (the same corner the sprite is drawn from). `collides_with` is an array of tags: the entity's `on_collide(self, other)` is called on every tick its hitbox overlaps (strictly, touching edges do not count) the hitbox of an entity carrying one of those tags. Both read back as new tables, and both can change at any time, like `tags`. Collisions are one-sided: a missile with `collides_with = { "enemy" }` and an enemy with `collides_with = { "missile" }` each get their own call; an enemy without `collides_with` is never told. Both calls of a pair run even if the first destroys an entity, but an entity destroyed earlier in the tick gets no further collision. Detection runs in C++ (`engine-physics`) after every `update`, so it costs nothing in Lua.

```lua
entity "enemy_shot"

tags          = { "enemy_shot" }
hitbox        = { w = 24, h = 24 }
collides_with = { "player" }

function on_collide(self, other)
    self:destroy()
end
```

`self:destroy()` only marks the entity: it stays usable until the end of the tick, is skipped by later `update` calls, then `on_destroy(self)` runs and the entity is removed. Calling it twice has no extra effect. `on_spawn(self)` runs once the entity and its `spawn` overrides are in place.

`owner` takes a player handle or `nil`, also as a `spawn` override (`spawn("ship", { owner = player })`). The entity then reads that player's input through `self.input`, and clients receive the owner so each one can highlight its own entities. Once the player leaves, `owner` reads `nil`.

**Cameras.** `zoom`, `follow` and `bounds` make an entity a camera (a `Camera` component): its `x`, `y` is the world point shown at the center of the screen. `zoom` is a positive number (default 1, 2 shows the world twice as big), `follow` a tag: every tick, after the systems, the server moves the camera onto the entity with that tag and the lowest id (an empty string, the default, or no such entity leaves it in place). `bounds = { x, y, w, h }` is a world area the view never leaves; on an axis where the view is larger than the area it is centered on it, and `w = 0, h = 0` (the default) means no bounds. `zoom` and `bounds` are replicated, `follow` stays on the server. A scene without camera keeps the default view, and camera entities are never drawn. A camera entity cannot be called `camera`, which is a reserved global ([Reserved names](#reserved-names)).

```lua
entity "main_camera" {
    x = 960, y = 540,
    zoom   = 1,
    follow = "player",
    bounds = { x = 0, y = 0, w = 8000, h = 1080 },
}
```

Native fields are aliases of component fields declared in the engine's field registry: `x` / `y` are the axes of `Position.pos`, `vx` / `vy` of `PhysicsBody.velocity`, `sprite` is `Sprite.path`, `color` is `Sprite.color`, `z_index` is `Layer.z_index`, `rotation` is `Rotation.degrees`, `scale` is `Scale.x` with `Scale.y`, `tags` is `Tags.labels`, `hitbox` is `AABBCollider.size`, `collides_with` is `CollidesWith.labels`, `zoom` is `Camera.zoom`, `follow` is `Camera.follow` and `bounds` is `Camera.bounds_origin` with `Camera.bounds_size`. `get_field` / `set_field` reach every other registered field by component and field name, with the same conversions:

| Field kind | Lua value |
|---|---|
| `float` | finite number |
| `int` | whole number (`4` or `4.0`, not `4.5`) |
| `bool`, `string` | boolean, string |
| `asset` | path string; an empty path reads `nil` |
| `vec2` | `{ x = .., y = .. }` |
| `color` | `{ r, g, b, a }`, numbers in [0, 255], `a` defaults to 255 |
| `list` | array of one kind, e.g. strings; reads an empty array when the component is absent |

Every value read is a new table or a copy: change a field by assigning it. A value of the wrong type, `NaN`, infinity or a value the component rejects raises a Lua error naming the field and the caller's line (`enemy.lua:12: entity field 'hitbox': size must be positive`), and leaves the entity unchanged.

Setting `vx`, `vy` or `hitbox` on an entity without a physics body gives it one: dynamic, without gravity and without collision response. Until then `vx` and `vy` read `nil`, so write `(self.vx or 0)` when an entity may not have moved yet.

**Performance tip:** prefer setting `vx` / `vy` over moving the entity in `update`; native movement runs entirely in C++.

## Global functions

```lua
-- entities
spawn("enemy", { x = 1900, y = 300, speed = 200 })   -- optional field overrides
find_all("enemy")          -- entities with a tag
find_first("player")
count("enemy")

-- players
players()                  -- connected players, by ascending id

-- scenes (server)
load_scene("stage2")       -- switch to scenes/stage2.json at the end of the tick
find_player(1)             -- player handle, or nil

-- time
after(2, function() ... end)     -- run once after 2 s
every(0.5, function() ... end)   -- run every 0.5 s (returns a handle to cancel it)
wait(1.5)                        -- only inside a coroutine

-- sound (replicated to clients)
sounds { "shot.wav", "explosion.wav" }   -- declare, while the scripts load
musics { "stage1.ogg" }
play("shot.wav")                 -- play a declared sound
play("explosion.wav", self)      -- with the entity that makes it
music("stage1.ogg")              -- loop a declared music, replacing the current one
music(nil)                       -- stop the music

-- textures (replicated to clients by id)
textures { "sprites/explosion.png", "sprites/boss/core.png" }   -- declare, while the scripts load
self.sprite = "sprites/boss/core.png"                           -- any declared texture

-- shaders (replicated to clients by id; files are <name>.fs and an optional <name>.vs)
shaders { "shaders/space", "shaders/hit_flash" }                -- declare, while the scripts load
self.shader = "shaders/hit_flash"                               -- any declared shader, "" for none

-- game events (sent to clients)
event "explosion" { at = "vec2", size = "float" }   -- declare, while the scripts load
emit("explosion", { at = { x = self.x, y = self.y }, size = 2 })
emit("explosion")                -- arguments left out are simply absent
on("explosion", function(event) ... end)   -- client only: react to a received event

-- local entities and HUD (client only, see [Networking from the script's point of view](#networking-from-the-scripts-point-of-view))
spawn_local("explosion", { x = 100, y = 200 })   -- never sent to the server
draw.text("SCORE 42", 30, 28, 24)                 -- only inside client draw_hud()

-- local input and audio (client only)
input:pressed("M")               -- true on the tick the key goes down
input:key("Space")               -- held keys, like self.input on the server
audio.set_muted(not audio.muted())
audio.set_volume("music", 0.5)   -- "music" or "effects", from 0 to 1

-- pathfinding
navgrid { width = 1920, height = 1080, cell = 32, obstacles = { "wall" } }
find_path(self, { x = 1500, y = 300 })                  -- array of { x, y }, or nil
find_path(a, b, { avoid = { "lava" }, max_nodes = 500 })

-- console variables (see [Developer console and console variables](#developer-console-and-console-variables))
cvar.register { name = "game.lives", default = 3, min = 0, max = 9, description = "lives at start" }
cvar.get("game.lives")           -- value, or nil and an error log for an unknown name
cvar.set("debug.hitboxes", true) -- true, or false and an error log
cvar.reset("game.lives")
cvar.list()                      -- names, sorted

-- utilities
random(1, 10)
world.width, world.height        -- 1920, 1080
log("message")
```

**Sound rules.**

- `sounds` and `musics` take a list of file names and can only be called while the scripts load (top level of any file, or `on_start`). A name is 1 to 63 letters, digits, `_`, `-` or `.`, not starting with `.`. A bad or already declared name raises an error and declares none of the names of that call. Clients look for sounds in `assets/sounds/` and musics in `assets/music/`.
- `play` and `music` with an undeclared name, or with arguments of the wrong type, raise an error. Nothing is played on the server: the requests are queued, and after each tick the server sends them to every client (`sound` and `music` events, see the [protocol](../protocol.md)). At most 64 requests are kept per tick.
- The current music is also sent to players who join later.

**Texture rules.**

- `textures` takes a list of paths relative to `assets/` and can only be called while the scripts load. A name is 1 to 63 bytes of `/`-separated folders and a file, each made of letters, digits, `_`, `-` or `.` and not starting with `.` (no absolute path, no `..`). A bad name raises an error and declares none of the names of that call; declaring a name twice is allowed.
- The `sprite` of every `entity` declaration is declared automatically; an entity whose `sprite` is not a valid name fails the load.
- The server sends the declared textures once in `connect_accept`; snapshots then carry `sprite` as a 16-bit texture id, so the client only ever loads files the server declared.
- `shaders` follows the same rules with the names of shaders (no extension) and the `shader` field; shaders and textures are separate lists, so a shader name is not a texture and the other way round.

**Event rules.**

- `event "name" { key = "kind", ... }` declares a game event while the scripts load. A name is 1 to 63 letters, digits, `_`, `-` or `.`, starting with a letter (`boss.died`). Keys are Lua identifiers of 1 to 63 characters; kinds are `float`, `int`, `bool`, `string`, `vec2`, `color` or `asset` (no `list`). An event has at most 16 keys. A bad name, key or kind, a name declared twice or a declaration after loading raises an error.
- `emit(name, arguments)` queues a declared event. `arguments` is a table or `nil`; keys left out are absent from the event. An undeclared event or key, a value of the wrong kind (same conversions as entity fields), a non-finite number, a fractional `int` or a `string`/`asset` longer than 63 bytes raises an error and emits nothing. An `asset` argument travels as a texture id: the server drops, with a warning, an event naming a texture that is not declared. At most 64 events are kept per tick.
- After each tick the server sends the queued events to every client (`named` events, see the [protocol](../protocol.md)), after that tick's sounds and musics. Event names and keys are sent once in `connect_accept`; a client publishes each event on its event bus as `engine::network::NamedEvent`.
- On the client, `on(name, function(event) ... end)` reacts to a declared event: the function gets a table of the event's arguments (same conversions as entity fields), handlers run in registration order, and an error in one is logged without stopping the others. `on` with an undeclared event or a non-function raises an error.
- `scene_loaded { name = "string" }` is built in: it always exists, is sent by `load_scene`, and declaring it raises an error.

**Scene rules.**

- `load_scene(name)` (server only) switches to `scenes/<name>.json` of the game folder. The file is read and checked when it is called: a name that is not 1 to 63 letters, digits, `_` or `-`, a missing or invalid file, a server started without a game folder, or a second `load_scene` in the same tick raises an error and keeps the current scene. Problems inside a valid file (unknown type, bad override) are logged and skip only that instance or field.
- The switch happens at the end of the tick, after the entities marked with `self:destroy()` are removed: every entity whose declaration does not set `persistent = true` is destroyed (`on_destroy` runs), the scene's instances are spawned (`on_spawn` runs, then their overrides), the `scene_loaded` event is queued for clients, and `on_scene_loaded(name)` from `main.lua` is called. A `load_scene` called during these callbacks switches at the end of the next tick.
- Persistent entities keep their id and their Lua data. Fields that point to destroyed entities are left as they are; using one raises the usual "destroyed entity" error, so clean them in `on_scene_loaded`. A persistent entity placed in a scene file is spawned again each time that scene loads, so persistent entities are usually spawned by scripts.
- `persistent` must be a boolean set at the top level of the declaration (not in a `server`/`client` section); anything else rejects the file. It is not a script field.
- `main.lua` lives across scenes. On the client, `on_scene_loaded(name)` from `main.lua` is called when the `scene_loaded` event arrives, before the `on("scene_loaded", ...)` handlers. The start scene of `game.json` does not fire it: `on_start` covers it.

**Pathfinding rules.**

- `navgrid { ... }` lays a grid of square cells over an area of the world and chooses how paths are searched. It can be called at any time (usually in `on_start`); a later call replaces the grid. A bad option raises an error and keeps the previous grid.

| Option | Default | Meaning |
|---|---|---|
| `width`, `height` | required | Size of the area, in world units |
| `x`, `y` | `0`, `0` | Top-left corner of the area |
| `cell` | `32` | Side of a cell; at most 2^20 cells in total |
| `obstacles` | `{}` | Tags of the entities that block cells |
| `padding` | `0` | Grows each obstacle by this many units on every side, so wide entities keep clear of walls |
| `heuristic` | `"octile"` | `"octile"`, `"manhattan"`, `"euclidean"` or `"dijkstra"` |
| `diagonals` | `true` | Allow diagonal moves; a diagonal never cuts a wall corner |
| `max_nodes` | `10000` | Search budget per call: a search that explores more cells gives up |
| `smooth` | `true` | Skip the waypoints a straight line can cut across |

- An entity carrying one of the `obstacles` tags and a `hitbox` (or a circle collider) blocks every cell its collider, grown by `padding`, strictly overlaps; touching a cell's edge does not block it. The grid is rebuilt at most once per tick, the first time it is used, so obstacles spawned or moved during a tick count from the next tick on.
- `find_path(from, to, options)` returns the waypoints from `from` to `to` as an array of `{ x, y }`: the centres of the cells to cross, without the start, ending with `to` exactly. `from` and `to` are tables `{ x = .., y = .. }` or entities (the centre of their `hitbox`, or their `x`, `y` without one). It returns `nil` when either point is outside the grid or on a blocked cell, when no path exists, or when the search budget runs out. `options` may set `avoid` (extra obstacle tags for this call only) and `max_nodes`.
- `self:navigate_to(target, speed)` plans a path from the entity's centre to `target` (a point or an entity, taken where it is now) and returns `true`; the engine then sets the entity's `vx`, `vy` every tick to follow the waypoints at `speed` units per second (giving the entity a physics body if it has none), stops it on the target and calls its `on_arrive(self)`. Without a path it returns `false` and the entity stops. The path is not re-planned on its own: call `navigate_to` again to follow a moving target or after obstacles change (every few tenths of a second is plenty). `self:stop_navigation()` stops the entity; `self:is_navigating()` tells whether it is following a path.
- Calling `find_path` or `navigate_to` before `navgrid`, a point that is not an entity or a table of two finite numbers, an unknown option, a `speed` that is not a positive number or a `max_nodes` that is not a whole number from 1 to 10 000 000 raises an error.

## Systems

A system handles logic that spans several entity types.

**Global system** (no query, runs once per tick):

```lua
-- stage1/waves.lua
system "waves"

local timer = 0

function update(dt)
    timer = timer + dt
    if timer > 2 then
        timer = 0
        spawn("enemy", { x = world.width + 50, y = random(100, 980) })
    end
end
```

**Query system** (runs for every entity that has the listed fields):

```lua
-- common/gravity.lua
system "gravity"

query = { "gravity" }

function update(e, dt)
    e.vy = (e.vy or 0) + e.gravity * dt
end
```

Any entity that defines `gravity = 300` is affected, without declaring anything else.

**Event system:**

```lua
-- common/score.lua
system "score"

score = synced(0)

function on_destroy(e)
    if e.points then score = score + e.points end
end
```

**Rule of thumb:** if the logic only concerns one entity type, put it in that entity; if it applies to several, write a system.

## Player input

Clients send their **raw keyboard and mouse state** every tick: every key and mouse button held, and the mouse position in virtual units. The engine gives keys no meaning; the game's scripts decide what each key does, so bindings are plain Lua and nothing is declared or kept in sync with the client.

```lua
function update(self, dt)
    local input = self.input
    if input:key("Space") or input:mouse("Left") then
        spawn("player_missile", { x = self.x + 30, y = self.y, owner = self })
    end
    local x, y = input:pointer()
end
```

`self.input` is available on every entity and is read-only:

| Call | Returns |
|---|---|
| `input:key(name)` | `true` while the key is held. Names are the engine's `Key` enumerators: `"A"` to `"Z"`, `"Num0"` to `"Num9"`, `"Space"`, `"Enter"`, `"Left"`, `"Up"`, `"LeftShift"`, `"F1"`... (case-sensitive) |
| `input:mouse(name)` | `true` while the mouse button is held: `"Left"`, `"Right"` or `"Middle"` |
| `input:pointer()` | the mouse `x, y` in whole virtual units, or `nil` when the client sends none |

An unknown key or button name (`input:key("Spcae")`) raises a Lua error. An entity reads the input of its `owner`; entities without one read a neutral input (nothing held, no pointer).

Actions are a game-side convenience: the [R-Type example](../examples/r-type.md) keeps its bindings in `lib/controls.lua` and reads `controls.pressed(input, "fire")` and `controls.axis(input, "move_x")`:

```lua
-- lib/controls.lua (excerpt)
local actions = {
    fire = { keys = { "Space" } },
}

local axes = {
    move_x = { negative = { "Left", "Q" }, positive = { "Right", "D" } },
    move_y = { negative = { "Up", "Z" }, positive = { "Down", "S" } },
}
```

Players are handled by two optional callbacks of `main.lua`. The engine calls them when a client connects or leaves; scripts never see the network:

```lua
function on_player_joined(player)
    spawn("ship", { x = 100, y = 120 + player.slot * 140, owner = player })
end

function on_player_left(player)
    for _, owned in ipairs(player:entities()) do
        owned:destroy()
    end
end
```

A player handle has `id` (stable while connected), `slot` (lowest free index from 0, reused after a departure), `connected`, and `player:entities()` (live entities it owns). Without `on_player_left`, the engine destroys the player's entities itself. Errors in either callback are logged and the game continues.

Keys travel as a 128-bit set indexed by `Key` value, mouse buttons as 3 bits, the pointer as two 16-bit ints; see [protocol.md](../protocol.md). Remapping is a Lua change on the server; per-player remapping and client-side bindings come with client scripts.

## Coroutines

The special callback `behavior` runs as a coroutine: `wait()` pauses only this entity, never the game.

```lua
entity "boss"

sprite = "boss.png"
health = synced(50)

function behavior(self)
    while true do
        self:move_to(1500, 300, 2)
        for i = 1, 5 do
            self:shoot()
            wait(0.2)
        end
        wait(1)
    end
end
```

## Networking from the script's point of view

- Scripts are written once; the developer does not write network code.
- Game logic (`update`, `on_collide`, `behavior`, systems) runs **on the server only**. The client runs the same scripts: its `main.lua` code (`client = { ... }`) reacts to events, reads the local input, changes the audio levels, spawns **local** entities for effects and draws the HUD. Local entities never reach the server or the other clients.
- `play`, `music` and `emit` are sent to clients as events; clients receive the declared names (and event keys) in `connect_accept`.
- Native fields are replicated automatically. A free field is replicated only when marked:

```lua
health = synced(3)   -- visible on clients (e.g. health bar)
speed  = 150         -- server only
```

- Functions and scripts are never sent over the network, only data.

**Which side has which function.** Every runtime has the **core** API; each side adds its own **API set**, and a function of the other side simply does not exist there, so calling it is an ordinary Lua error (`attempt to call a nil value (global 'spawn')`):

| Set | Globals and fields | Runtime |
|---|---|---|
| `core` | `entity`, `system`, `require`, `sounds`, `musics`, `textures`, `shaders`, `event`, `navgrid`, `find_path`, the sandboxed standard libraries, entity fields and methods of [Entity API (`self`)](#entity-api-self) except `input` and `owner` | every runtime |
| `server` | `spawn`, `players`, `find_player`, `play`, `music`, `emit`, `load_scene`, `self.input`, `self.owner`, `on_player_joined`, `on_player_left`, `on_scene_loaded` | server (`core` + `server`) |
| `client` | `on`, `input`, `audio`, `camera`, `spawn_local`, `draw`, `draw_hud`, `on_scene_loaded` | client (`core` + `client`) |
| `editor` | nothing yet | editor (`core` + `editor`) |

Top-level code of a file runs on every side, so it may only use `core` functions; side-specific calls belong in callbacks.

**Sections.** A declaration can hold `server = { ... }` and `client = { ... }` tables. Each side merges its own section into the prefab and ignores the other one; neither name is visible as a field afterwards. Logic that must only run on the server goes in `server`:

```lua
entity "enemy" {
    sprite = "enemy.png",
    hitbox = { w = 32, h = 32 },

    server = {
        health = 3,
        update = function(self, dt) self.x = self.x - 150 * dt end,
    },

    client = {
        trail = "red",
    },
}
```

Short forms work the same way (`server = { ... }` at the top of the file). A section that is not a table, or that sets a name the declaration already sets outside it (`speed = 1` and `server = { speed = 2 }`), rejects the whole file on every side. The same name in both sections is fine.

`main.lua` follows the same rule with its globals, so each side gets its own callbacks:

```lua
server = {
    on_start = function() spawn("game") music("stage1.ogg") end,
    on_player_joined = function(player) ... end,
}

client = {
    update = function(dt)                 -- every client tick, 60 per second
        if input:pressed("M") then audio.set_muted(not audio.muted()) end
    end,
}
```

**Client input and audio.** `input` is the local keyboard and mouse of the current tick. `input:key(name)`, `input:mouse(name)` and `input:pointer()` read it like `self.input` on the server; `input:pressed(name)` is true only on the tick the key goes down, for toggles. An unknown key or button name raises an error. `audio.muted()`, `audio.set_muted(bool)`, `audio.volume(kind)` and `audio.set_volume(kind, volume)` read and change the client's audio levels; `kind` is `"music"` or `"effects"`, `volume` a number from 0 to 1, and anything else raises an error and changes nothing. The levels start from the command line (`--music-volume`, `--effects-volume`, `--mute`).

**Client camera.** By default the client draws through the scene camera ([Entity API (`self`)](#entity-api-self)). Its scripts can override it **locally**, without affecting the server or the other clients:

```lua
camera.set(x, y [, zoom])          -- show the world point (x, y) at the center of the screen
camera.follow(id [, zoom])         -- center on the replicated entity whose self.id is id
camera.reset()                     -- back to the scene camera (or the default view)
local wx, wy = camera.screen_to_world(input:pointer())   -- world point under the mouse
```

A missing `zoom` keeps the zoom of the current view. `camera.follow` keeps the scene camera's bounds and falls back to the scene camera while the entity is not replicated; the server sends ids to clients through events, e.g. `emit("you_are", { id = self.id })`. `input:pointer()` stays in screen units; `camera.screen_to_world` converts through the view drawn this frame. A coordinate that is not a finite number, a `zoom` that is not positive or an `id` that is not a non-negative integer raises an error and changes nothing.

**Local entities.** `spawn_local(type [, overrides])` spawns a declared entity on this client only, with its `client` section merged in, exactly like `spawn` on the server: it returns the entity, or `nil` (logged) for an unknown type. Local entities are never replicated. Every client tick runs their `update(self, dt)`, moves them by their `vx` / `vy`, runs `on_collide` between local entities, then removes those destroyed with `self:destroy()`. They are drawn with the replicated entities, in the world and through the camera, sorted by `z_index` ([Entity API (`self`)](#entity-api-self)). Their `self.id` is only unique among local entities: `camera.follow` always means a replicated id.

```lua
entity "spark" {
    color = { r = 255, g = 200, b = 0 },
    z_index = 10,
    client = {
        update = function(self, dt)
            if self.age > 0.3 then self:destroy() end
        end,
    },
}

client = {
    on_start = function()
        on("explosion", function(event)
            spawn_local("spark", { x = event.at.x, y = event.at.y, vx = 80 })
        end)
    end,
}
```

**HUD.** A `draw_hud()` callback in the `client` section of `main.lua` runs once per frame, after the world is drawn. Inside it, `draw.*` queue drawings in **screen space**: virtual units of the 1920 × 1080 screen, top-left origin, never moved or zoomed by the camera. They are drawn in call order, in front of the world.

```lua
local score = 0

client = {
    on_start = function()
        on("scored", function(event) score = event.total end)
    end,
    draw_hud = function()
        draw.rect(20, 20, 300, 40, { r = 0, g = 0, b = 0, a = 160 })          -- x, y, w, h [, color [, filled]]
        draw.text("SCORE " .. score, 30, 28, 24, { r = 255, g = 255, b = 255 }) -- text, x, y, size [, color]
        draw.sprite("ui/heart.png", 1800, 20, { scale = 2 })                   -- texture, x, y [, { scale, rotation, color }]
    end,
}
```

- Colors default to white. `filled = false` draws the outline only. `draw.sprite` takes a texture declared with `textures { ... }`; `scale` is a number or `{ x, y }` (negative mirrors), `rotation` is in degrees clockwise around the center, and the scaled sprite keeps its top-left corner at `x`, `y`.
- A coordinate that is not a finite number, a negative `w` or `h`, a `size` that is not positive, a text that is not a string or is longer than 256 bytes, an undeclared texture, or a call outside `draw_hud()` raises an error and queues nothing.
- An error in `draw_hud()` is logged; what it drew before the error is still shown and the frame goes on. At most 1024 drawings are kept per frame; later ones are dropped and logged once.
- The callback is called `draw_hud`, not `draw`, because the `client` section's names become globals of `main.lua` and would hide the `draw` table.

## Server tick order

1. Apply received inputs.
2. Run Lua systems.
3. Run entity `update` callbacks and resume coroutines; fire `after` / `every` timers.
4. Steer navigating entities (`navigate_to`) → `on_arrive` callbacks.
5. Native movement (`vx`, `vy`).
6. Collisions (`engine-physics`) → `on_collide` callbacks.
7. Destroy entities marked for destruction → `on_destroy` callbacks.
8. Switch scene if `load_scene` was called → `on_destroy`, `on_spawn`, `on_scene_loaded`.
9. Send snapshot to clients.

## Reserved names

Developers must not use these names for their own fields or methods; the engine warns at load time if they do.

- Native fields: `x`, `y`, `vx`, `vy`, `sprite`, `color`, `z_index`, `hitbox`, `collides_with`, `tags`, `zoom`, `follow`, `bounds`, `layer`, `age`, `id`, `type`, `input`, `owner`.
- Declaration sections: `server`, `client`. Declaration flag: `persistent`.
- Entity methods: `destroy`, `has_tag`, `get_field`, `set_field`, `navigate_to`, `stop_navigation`, `is_navigating`.
- Callbacks: `on_spawn`, `update`, `on_collide`, `on_destroy`, `behavior`, `on_arrive`; in `main.lua`: `on_start`, `on_player_joined`, `on_player_left`, `on_scene_loaded`, `update`, `draw_hud`.
- Declarations and globals: `cvar`, `entity`, `system`, `query`, `spawn`, `find_all`, `find_first`, `count`, `after`, `every`, `wait`, `play`, `music`, `sounds`, `musics`, `event`, `emit`, `random`, `world`, `log`, `synced`, `players`, `find_player`, `on`, `input`, `audio`, `camera`, `spawn_local`, `draw`, `textures`, `shaders`, `navgrid`, `find_path`, `load_scene`.
- Events: `scene_loaded`.

## Error handling and sandbox

- Every call into Lua is protected. An error is logged with the file and line (`enemy.lua:14: attempt to index nil value 'target'`), the faulty entity or system is disabled, and the game continues.
- Only safe Lua libraries are opened: `base`, `math`, `string`, `table`, `coroutine`. No `io`, no `os`: scripts cannot access the file system or run commands.
- Common mistake to document: call methods with `self:method()`, not `self.method()`.
- Mistakes the engine would otherwise ignore silently are logged as warnings while the scripts load, and the editor lists them in its Problems tab:
  - a name defined beside a table-form declaration (`entity "player" { ... }` then `function update(self, dt)` below it): it belongs to neither, so it is ignored. Move it into the table, or use the short form;
  - a function close to a callback name, which the engine never calls (`on_collision`, "did you mean 'on_collide'?"; `updte`, "did you mean 'update'?"). Custom methods with other names, such as `on_hit` or `fire`, are not reported.
- Calling a planned but unimplemented function (`after`, `every`, `wait` and the others of the status table above) fails like any nil call, and the first line of the error adds `('after' is planned but not implemented yet, see docs/reference/lua-api.md)`.

## How it maps to C++ (for engine developers)

- **Declarations** — every declaration file runs in its own `sol::environment` whose `__index` is the globals table. `entity "name"` / `system "name"` record a pending declaration and return a declarator; calling the declarator with a table (`entity("name")({ ... })`, written `entity "name" { ... }`) stores that table as the prefab. Once the file has run, an uncalled declarator (short form) takes the file environment as its prefab. A file with one short form, or only table forms, is registered as a whole; two short forms, or a short form mixed with table forms, rejects the file. The registry maps each name to its prefab table, kind and source path.
- **API sets** — `ScriptRuntime` holds the sandbox, the loader and the declarations; `ScriptRuntime::builder().with<ServerApi>().build()` adds a set, which binds its globals and entity handle fields (`Entities::add_handle_field`) on an internal `RuntimeContext` when the runtime is built. The runtime owns its sets; the host reaches one with `runtime.api<ServerApi>()`. Each set's `section()` names the declaration section merged into prefabs (`loading/Sections.cpp`) before they are registered.
- **Global functions** are C++ lambdas exposed with sol2:

  ```cpp
  lua.set_function("spawn", [&](const std::string& type, sol::optional<sol::table> overrides) {
      return world.spawn(type, overrides);
  });
  ```

- **Field access** goes through the entity handle's metatable. `__index` looks up, in order: native field aliases → entity methods → the entity's Lua data → its prefab table (methods). `__newindex` writes native fields into C++ storage and everything else into the Lua data. Aliases are a small data table (`engine/src/script/EntityFields.cpp`) mapping a Lua name to a `core::FieldRegistry` field, resolved to a `FieldRef` once; `sol::object` ↔ `FieldValue` conversion lives in one place (`LuaFieldValue.cpp`). The registry, filled by `register_core_fields` and `register_physics_fields`, is exposed by `ScriptRuntime::fields()`. Setting a collider field also adds the `PhysicsBody` it requires (`FieldRegistry::add(..., required<PhysicsBody>)`); the `hitbox` alias additionally makes a new body dynamic without gravity or collision response, centres the collider (`offset = size / 2`) and adds `Scriptable`.
- **Instances** — `ScriptRuntime::capture_instances()` / `spawn_instances()` (`entity/Instances.cpp`) turn the world into `Instance{name, type, position, overrides}` and back. A type's defaults come from a prototype entity built in a scratch world from the prefab's native fields, without `on_spawn`; only editable fields that differ are stored. Script fields (`entity/ScriptFields.cpp`) are inferred from each entity prefab before its sections are merged, kept on the `Declaration`, compared with the entity's Lua data and written back into it as `"Script.<name>"` overrides; `ScriptRuntime::script_fields()` lists them. See [Engine modules](../engine.md#instances).
- **Scenes** — `ScriptRuntime::read_scene()`, `write_scene()` and `load_game()` (`scene/`) store instances in `scenes/<name>.json` and spawn the `start_scene` of `game.json`. `nlohmann-json` is private to the module. `load_scene` (`scene/SceneSwitch.cpp`) reads the file into `RuntimeContext::pending_scene`; `ScriptRuntime::flush_scene()`, called by `ServerApp` after `flush_destroyed()`, dooms every non-persistent entity (`Declaration::persistent`), flushes them, spawns the instances and queues the `scene_loaded` event that `Events` registers in its constructor. See [Game folder and scenes](../guide/game-folder.md).
- **Editor** — `engine-editor` (`engine/src/editor/`) loads a game folder with `ScriptRuntime::load_declarations()`, which runs the declaration files but neither `on_start` nor the start scene. `EditorSession` keeps the open scene as a `SceneDocument` (instances with session-only `InstanceId`s); every edit records a copy of the document in `History` (100 entries) and rebuilds the edit world with `ScriptRuntime::preview_instances()`, which builds prototypes in a given world without any Lua callback. `set_field` previews the single override in a scratch world before recording it. See [Editor backend](../editor.md).
- **Callbacks** are called by native systems through `sol::protected_function`.
- **Coroutines** are `sol::thread` + `sol::coroutine`; `wait(t)` yields and the scheduler resumes the coroutine when `t` has elapsed.

## Developer console and console variables

The client opens a developer console with `` ` `` or `F1` (see [Play and debug](../guide/play-and-debug.md#developer-console)). A line runs, in order of precedence:

1. `help`, `clear`, `cvar [list] [filter]`, `cvar <name>`, `cvar <name> <value>`, `cvar reset <name>`: built-in commands;
2. `/server <lua>`: Lua run by the server of the same process (`solo` and `host` modes), before its next tick;
3. anything else: Lua run by the client runtime.

Console Lua is compiled as `return <line>` first, then as a statement, so `1 + 1` prints `2` and `x = 5` works. It reads and writes the globals of `main.lua` (the shared globals before scripts load); `local`s do not outlive the line. `print` is captured into the console. A syntax or runtime error is printed in red and nothing else happens: no entity or system is disabled.

**Console variables (CVars)** are named, typed values shared by the engine and the scripts, on the client and on the server; each side has its own registry.

- A name is lowercase letters, digits, `_` and `.`, starting with a letter (`game.lives`). The engine uses the prefixes `debug.`, `audio.` and `sv.`.
- `cvar.register { name, default, description, min, max, kind }` registers a variable; usually at the top level of a file, so it exists before `on_start`. Its kind is `bool`, `int`, `number` or `string`, taken from `default` (`3` is an `int`, `3.0` a `number`) unless `kind` says otherwise (`kind = "number"` with `default = 1`). `min` and `max` go together and only for numbers. A duplicate or invalid name, a bad default or range logs an error and returns `false`.
- `cvar.get(name)` returns the value; `cvar.set(name, value)` changes it if the kind matches (an `int` is accepted by a `number` variable, a whole `number` by an `int` one) and the value is within the range, else logs an error and returns `false`; `cvar.reset(name)` restores the default; `cvar.list()` returns the names.
- Scripts read variables when they need them (`cvar.get` in `update`), there is no change callback in Lua.

| Engine variable | Side | Kind | Effect |
|---|---|---|---|
| `debug.metrics` | client | bool | Metrics overlay, like `F3` |
| `debug.hitboxes` | client | bool | Outlines the colliders of the client's local entities |
| `audio.music_volume`, `audio.effects_volume` | client | number, 0–1 | Same as `audio.set_volume` |
| `audio.muted` | client | bool | Same as `audio.set_muted` |
| `sv.tick_rate` | server | int, 1–240 | Server ticks per second (60) |
