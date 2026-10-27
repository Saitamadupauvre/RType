# R-Type — Project Overview

> Networked multiplayer shoot'em up built on our own C++ game engine, with gameplay written in Lua.
> Target track for Part 2: **Track 1 — Advanced Game Engine**.

This page is the reference for the vision, the architecture decisions and the Lua API. It describes the **target design**; where the implementation is not there yet, the section says so. For the current state of the code, start with [Architecture](architecture.md).

---

## Table of contents

1. [Vision](#1-vision)
2. [Scope and deliverables](#2-scope-and-deliverables)
3. [Technology stack](#3-technology-stack)
4. [Architecture](#4-architecture)
5. [Repository layout](#5-repository-layout)
6. [Executables](#6-executables)
7. [Server model](#7-server-model)
8. [Networking](#8-networking)
9. [Graphics plugin](#9-graphics-plugin)
10. [Lua scripting API](#10-lua-scripting-api)
11. [Engine rules and conventions](#11-engine-rules-and-conventions)
12. [Accessibility](#12-accessibility)
13. [Quality: tests, CI, packaging](#13-quality-tests-ci-packaging)
14. [Documentation](#14-documentation)
15. [Roadmap](#15-roadmap)
16. [Open decisions](#16-open-decisions)

---

## 1. Vision

We are not only building a game; we are building a **reusable 2D multiplayer game engine** and using it to make R-Type.

- The **engine** is written in C++. It knows nothing about R-Type: it provides an ECS, a game loop, networking, rendering, audio, input and a Lua runtime.
- The **game** is written in **Lua**. A game developer describes entities and systems in script files; the engine loads and runs them.
- The server is **authoritative**: all game logic runs on the server. Clients send inputs and display what the server replicates.

The success criterion for the engine is simple: **a second, different game can be built by writing only Lua scripts and assets**.

---

## 2. Scope and deliverables

### Part 1 — Prototype (week 3)

A playable, networked R-Type prototype:

- Starfield scrolling background, timer-based (never tied to CPU speed).
- Up to 4 players, visually distinguishable (not only by colour).
- Players move with the arrow keys and shoot.
- Bydos (enemies) spawn randomly from the right side of the screen.
- Missiles, collisions, destruction.
- Some sound effects.
- Multithreaded authoritative server; survives client crashes and notifies other players.
- Binary UDP protocol, robust to malformed packets, documented RFC-style.
- Engine split into decoupled subsystems (rendering, networking, logic), built as shared libraries, with the graphics backend loaded as a runtime plugin.
- Gameplay written in Lua on top of the engine.

Progress is tracked in the [roadmap](#15-roadmap).

### Part 2 — Advanced Game Engine (week 6)

See [Roadmap](#15-roadmap). Main goals: stronger modularity, engine extracted as a standalone project, generic runtime, tooling (level editor, developer console, hot reload), and a second game.

### Common requirements (evaluated at both defenses)

- Cross-platform: Linux (required) and macOS, tested in CI from day one. Windows support is postponed: new code is not required to compile on Windows for now, and the CI has no Windows job.
- Dependencies handled only by a package manager; no library sources copied into the repository.
- Git workflow: branches, pull requests, reviews, issues, tags.
- Documentation in English, published online.
- Accessibility solutions (motor, visual, auditory, cognitive), see [Accessibility](accessibility.md).

---

## 3. Technology stack

| Area | Choice | Why |
|---|---|---|
| Language | C++20 | Required by the subject; concepts, designated initializers, `std::span`. |
| Build system | CMake | Standard, cross-platform, integrates with package managers. |
| Package manager | vcpkg (manifest mode) | All our dependencies are ports; binary caching for CI; copies DLLs automatically on Windows. |
| Windowing / 2D rendering / input / audio | Raylib | One small library for all four; simple API. Isolated in a plugin. |
| Networking | POSIX sockets, wrapped in `engine-network` | No extra dependency; a small thread-safe UDP socket wrapper is all the protocol needs. |
| Scripting | Lua 5.4 + sol2 | Lightweight, designed to be embedded, industry standard for game scripting; sol2 gives a modern C++ binding. |
| Tests | GoogleTest | Decided. |
| Command line | clap (our own library, vcpkg overlay port) | Typed arguments with ranges, generated help. |
| Logging | Own logger in `engine-core` | Levels, `std::format` syntax, capturable in tests; no dependency. |
| Documentation | MkDocs Material + Doxygen | Markdown in the repository, published online with search; API reference from the public headers. |
| Formatting / CI | clang-format, clang-tidy, GitHub Actions | Linux GCC/Clang and macOS builds, sanitizers, static analysis. |

Every choice is compared with its alternatives in the [comparative study](comparative-study.md).

Linux system packages required by Raylib (X11, OpenGL, ALSA development headers) are low-level system dependencies and are listed in the README.

---

## 4. Architecture

### 4.1 Overview

```
            ┌───────────────────────── Engine (C++) ─────────────────────────┐
            │                                                                │
            │  engine-core     ECS, scheduler, events, time, fixed-step loop,│
            │                  logging, dynamic library loader               │
            │  engine-script   Lua runtime (sol2), entity/system loading     │
            │  engine-network  UDP sockets, protocol, threads, replication   │
            │  engine-physics  bodies, colliders, collision resolution       │
            │  engine-graphics renderer/input interfaces, plugin loader      │
            │                                                                │
            │  plugins/graphics-raylib   window, 2D rendering, input         │
            │                            (loaded at runtime, client only)    │
            └────────────────────────────────────────────────────────────────┘
                        │                                      │
             core + script + network            core + network + graphics
                        │                       + graphics plugin
                        ▼                                      ▼
                 r-type_server                          r-type_client
                        │                                      │
                        └──────────── game/ (Lua + assets) ────┘
```

### 4.2 Principles

- **The engine never depends on the game.** Nothing under `engine/` includes or references game code. The build enforces it: engine targets do not link against game targets.
- **Shared libraries everywhere.** Every engine module is a shared library (`.so` / `.dll`).
- **Runtime plugins for backends.** The graphics backend is a plugin loaded with `dlopen` / `LoadLibrary` through a small `DynamicLibrary` wrapper. The server never loads it, so it runs on machines without a display.
- **Third-party headers stay private.** `raylib.h` is included only inside the graphics plugin; POSIX socket headers only inside the private sources of the network module. This also avoids the well-known `windows.h` / Raylib name clashes on Windows.
- **Data flows through the ECS.** Subsystems communicate through components and an event bus, never by calling each other directly.

### 4.3 ECS

- Entities are IDs with a generation counter, so a recycled ID is never confused with a destroyed entity.
- Native components are stored in component pools (sparse sets) in C++ (position, velocity, sprite, hitbox, tags...).
- Fields defined by scripts live in Lua tables attached to the entity.
- Systems are either native (C++: movement, collisions, replication, rendering) or scripted (Lua).

### 4.4 World and time

- The world uses **virtual units** independent from the window: **1920 × 1080**, origin at the top-left, Y axis pointing down. The client scales this space to the window with letterboxing.
- The server runs a **fixed-timestep loop** (target: 60 ticks per second). Game logic never depends on frame rate or CPU speed.

---

## 5. Repository layout

### Part 1

```
r-type/
├── engine/
│   ├── cmake/              engine_add_module / engine_add_plugin
│   ├── include/engine/     public headers (core, graphics, network, physics, script)
│   └── src/                implementation
├── plugins/
│   └── graphics-raylib/    the only code that includes raylib.h
├── game/
│   ├── server/             → r-type_server (Cli, ServerApp, main.cpp)
│   ├── client/             → r-type_client (Cli, Controls, SnapshotView, Starfield, main.cpp)
│   ├── common/             input encoding shared by both
│   ├── sandbox/            → r-type_sandbox
│   └── scripts/            Lua game code (free layout, see §10)
├── assets/                 sprites
├── ports/                  vcpkg overlay ports (clap)
├── tests/                  GoogleTest suites mirroring engine/, game/, plugins/
├── docs/                   MkDocs sources
├── mkdocs.yml
├── vcpkg.json
└── CMakeLists.txt
```

### Build output

```
bin/
├── r-type_server
├── r-type_client
├── libengine-core.so
├── libengine-graphics.so
├── libengine-network.so
├── libengine-physics.so
├── libengine-script.so
├── plugins/
│   └── graphics-raylib.so
├── scripts/
└── assets/
```

Executables locate their libraries, plugins, scripts and assets **relative to their own location** (RPATH `$ORIGIN` on Linux, same folder on Windows), never relative to the current working directory, so the game still works once installed.

---

## 6. Executables

In Part 1, each executable has a short, explicit `main` that assembles engine modules. Nothing is hidden: reading it shows the whole tick or frame. Simplified from the real code:

```cpp
// game/server/ServerApp.cpp: one tick, run at 60 Hz by a FixedStepLoop
void ServerApp::tick(Duration step)
{
    network_.receive();                    // sessions, timeouts, latest input of each player
    apply_inputs(network_.inputs());       // to the Lua entities each player owns
    _runtime.update(dt);                   // every entity's update(self, dt)
    _runtime.flush_destroyed();            // on_destroy, then removal
    systems_.run(registry_, dt);           // native systems
    replicate();                           // mark scripted entities with a sprite or colour
    network_.send_snapshot(registry_);     // queued for the send thread
}
```

```cpp
// game/client/main.cpp
auto plugin = engine::graphics::load_graphics_plugin(executable_dir / "plugins" / GRAPHICS_PLUGIN_FILE);
engine::network::NetworkClient network(clock, events);
network.connect(host, port, ec);                       // exit 84 on failure
renderer.open({.width = 1280, .height = 720, .title = "R-Type"});
while (!renderer.should_close()) {
    network.update();                                  // snapshots, retries, heartbeats, server timeout
    loop.run_frame([&](Duration step) {                // fixed 60 Hz
        starfield.update(step);
        network.send_input(pack_input(read_controls(input)));
    });
    starfield.draw(renderer);
    view.draw(network.entities(), network.types(), network.player_id());
}
network.disconnect();
```

The client does not run Lua: it draws each entity with `assets/sprites/<type>.png`, using the type names sent by the server.

Usage:

```
./r-type_server <port> [--max-players <1-4>]
./r-type_client <server-ip> <port>
```

---

## 7. Server model

- **Authoritative:** clients send **inputs** (actions), never positions. The server simulates and replicates the result. This prevents trivial cheating.
- **Multithreaded:**
  - a **receive thread** reads datagrams, decodes and validates them, and pushes valid messages into the inbox queue; invalid packets are dropped and logged there and never reach the game;
  - the **game thread** drains the inbox at each tick, runs the simulation and pushes snapshots and events into the outbox queue;
  - a **send thread** sleeps until the outbox has messages, then encodes and sends them.
  The game loop never blocks waiting for a client or for the socket: it only touches the two queues. Only the receive thread reads from the socket and only the send thread writes to it.
- **Resilient:**
  - a client that stops sending heartbeats is disconnected after a timeout and the other players are notified;
  - a malformed packet is dropped and logged, never trusted;
  - a Lua error is caught, logged, and disables the faulty entity or system without stopping the game.

### Game flow (Part 1, minimal)

```
Waiting for players ──(start)──▶ Playing ──(all players dead)──▶ Game over ──▶ Waiting
```

Implemented in `game/scripts/game.lua`: the first player to join starts the round, later players get a ship at once, and the round restarts 3 seconds after the last ship is destroyed. Threads, queues and failure handling are detailed in [Networking internals](networking.md).

---

## 8. Networking

The protocol is specified RFC-style in [protocol.md](protocol.md) and its implementation is described in [Networking internals](networking.md). This section only lists the constraints the rest of the engine relies on:

- UDP for all gameplay traffic, binary encoding with a fixed endianness.
- Common packet header (at least: magic number, protocol version, message type, sequence number, payload size).
- Packets kept small enough to avoid IP fragmentation.
- Messages for at least: connection handshake, inputs, snapshots, entity spawn/destroy, sound events, heartbeat, disconnection.
- Entity types are sent as numeric IDs; the table *type name → ID* is sent at connection time.
- Every incoming packet is validated (size, type, bounds) before use.

The engine replicates the type of every replicated entity and each component field flagged `replicated` in the field registry it owns: for R-Type, position, velocity, owner and colour. Snapshot entities are `entity_id, type_id, [field_id, value]*`, field names are sent once in `connect_accept`, so a new replicated field needs no protocol change. Replicating Lua fields marked `synced(...)` is planned (see §10.10).

---

## 9. Graphics plugin

### 9.1 Interfaces

The engine only knows these interfaces (in `engine/include/engine/graphics/`). They use engine types (`Vec2`, `Rect`, `Color`, `TextureId`...) and never Raylib types.

```cpp
class IRenderer {
    // window
    virtual bool open(const WindowConfig&) = 0;
    virtual void close() = 0;
    virtual bool should_close() const = 0;
    virtual void set_fullscreen(bool) = 0;
    virtual Vec2 virtual_size() const = 0;
    // textures (called by the engine's resource manager, never by the game)
    virtual TextureId load_texture(const std::string& path) = 0;
    virtual void unload_texture(TextureId) = 0;
    virtual Vec2 texture_size(TextureId) const = 0;
    // frame
    virtual void begin_frame(Color clear) = 0;
    virtual void end_frame() = 0;
    // drawing, in virtual units
    virtual void draw_sprite(const SpriteDraw&) = 0;
    virtual void draw_rect(const Rect&, Color, bool filled = true) = 0;
    virtual void draw_circle(Vec2 center, float radius, Color) = 0;
    virtual void draw_line(Vec2 from, Vec2 to, float thickness, Color) = 0;
    virtual void draw_text(const std::string&, Vec2 pos, float size, Color) = 0;
    virtual Vec2 measure_text(const std::string&, float size) const = 0;
};

class IInput {   // raw keys; the engine's InputMap turns them into actions
    virtual bool is_key_down(Key) const = 0;
    virtual bool is_key_pressed(Key) const = 0;
    virtual bool is_key_released(Key) const = 0;
    virtual Key  last_key_pressed() = 0;   // for the key-remapping screen
    virtual Vec2 mouse_position() const = 0;
    virtual bool is_mouse_down(MouseButton) const = 0;
    virtual bool is_mouse_pressed(MouseButton) const = 0;
    virtual bool is_mouse_released(MouseButton) const = 0;
};

class IGraphicsBackend {   // what a plugin provides
    virtual IRenderer& renderer() = 0;
    virtual IInput& input() = 0;
};
```

### 9.2 Plugin ABI

A graphics plugin exports three C functions (no C++ name mangling):

```cpp
extern "C" std::uint32_t engine_graphics_api_version();
extern "C" IGraphicsBackend* engine_graphics_create();
extern "C" void engine_graphics_destroy(IGraphicsBackend*);
```

The loader refuses a plugin built for another API version, and the backend is always destroyed by the plugin that allocated it.

### 9.3 Rules

- All graphics calls happen on the **main thread** (the OpenGL context lives there). Network threads push data into queues read by the main thread.
- Timing comes from the engine clock, not from the backend.
- The client draws in virtual units; the backend handles scaling and letterboxing.

### 9.4 Audio plugin

Audio is a separate module (`engine-audio`) with its own plugin, so the sound library can change without touching graphics. Today `audio-raylib` uses Raylib's audio; another library (miniaudio, OpenAL, SDL_mixer...) only needs a new plugin.

```cpp
class IAudio {   // engine/include/engine/audio/IAudio.hpp
    virtual bool open() = 0;                 // false: no device, the backend stays silent
    virtual void close() = 0;
    virtual bool is_open() const = 0;
    virtual void update() = 0;               // every frame: feeds the music stream
    virtual SoundId load_sound(const std::filesystem::path&) = 0;
    virtual void unload_sound(SoundId) = 0;
    virtual void play_sound(SoundId, float volume, float pitch) = 0;   // effects overlap
    virtual MusicId load_music(const std::filesystem::path&) = 0;
    virtual void unload_music(MusicId) = 0;
    virtual void play_music(MusicId, bool loop) = 0;                    // one music at a time
    virtual void stop_music() = 0;
    virtual void set_master_volume(float) = 0;
    virtual void set_music_volume(float) = 0;
    virtual void set_effects_volume(float) = 0;
};
```

- Volume heard = call volume × effects (or music) volume × master volume, each clamped to [0, 1].
- A failed load returns the default id; playing it, or any unknown id, does nothing.
- The plugin exports `engine_audio_api_version`, `engine_audio_create` and `engine_audio_destroy`; `load_audio_plugin()` checks them like the graphics loader. `engine_audio_create` must not open the device.
- `NullAudio` (in `engine-audio`) plays nothing: the client uses it when the plugin cannot be loaded.
- Main thread only, like graphics.

---

## 10. Lua scripting API

This section is the specification of the Lua API. Not everything is implemented yet:

| Feature | Status |
|---|---|
| `entity "name"` declarations (short and table forms), `require` from `lib/`, sandbox | Implemented |
| `system "name"` declarations | Parsed and validated; systems do not run yet |
| `spawn(type, overrides)`, `players()`, `find_player(id)` | Implemented |
| `main.lua`: `on_start`, `on_player_joined`, `on_player_left` | Implemented |
| Entity fields `x`, `y`, `vx`, `vy`, `sprite`, `color`, `tags`, `hitbox`, `collides_with`, `id`, `type`, `age`, `input`, `owner`; free fields | Implemented |
| Entity methods `destroy()`, `has_tag()`, `get_field()`, `set_field()`; callbacks `on_spawn`, `update`, `on_collide`, `on_destroy` | Implemented |
| Native collisions: `engine-physics` detects `hitbox` overlaps in the server tick and calls `on_collide` (see §10.5) | Implemented |
| Native movement from `vx` / `vy` in the server tick (`engine-physics` integrates the body velocity) | Implemented |
| `sounds { ... }`, `musics { ... }`, `play(name, entity)`, `music(name)` | Implemented (see §10.6) |
| `find_all`, `find_first`, `count`, `after`, `every`, `wait`, `behavior`, `random`, `world`, `log`, `synced`, `add_tag`, `remove_tag`, `distance_to`, `move_to` | Planned (names already reserved) |

[Writing the game](writing-the-game.md) shows how the current game works around what is missing.

### 10.1 Principles

- The whole game is written in Lua. The developer never touches C++.
- **Files are free to live anywhere** in the game folder. The engine loads every `.lua` file recursively.
- **Each file declares what it contains**: `entity "name"` or `system "name"`. A file usually declares one thing; small or related things can share a file using the table form.
- A file uses **either** one short form (`entity "name"` alone, the whole file is the prefab) **or** any number of table forms (`entity "name" { ... }`). Two short forms in one file, or a short form mixed with table forms, is a load-time error and none of the file's declarations are registered.
- A file outside `lib/` that declares nothing (other than `main.lua`) is loaded but logs a warning.
- Names are **unique**. Two declarations with the same name are a load-time error that shows both file paths.
- `main.lua` at the root is the entry point.
- Utility code used with `require` lives in `lib/` and is **not** loaded automatically.
- `require("name")` loads `lib/name.lua`; dots are folders (`require("enemies.wave")` loads `lib/enemies/wave.lua`). Each module runs once, in its own environment, and its return value is cached (`true` if it returns nothing). Names only contain letters, digits, `_` and `.`, so a script cannot leave `lib/`. Modules cannot declare entities or systems, circular requires are an error, and precompiled bytecode is rejected. The standard `package` library is not available.

### 10.2 Loading order

1. Load every declaration file (entities and systems are registered, nothing is spawned).
2. Call `on_start()` from `main.lua`. `on_player_joined(player)` and `on_player_left(player)` are called later, whenever a player connects or leaves.

Since `spawn` is only used once the game has started, file order never matters.

### 10.3 Declaring an entity

**One entity per file** (default style):

```lua
-- stage1/bydo.lua
entity "bydo"

-- native fields: handled by the C++ engine
sprite        = "bydo.png"
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

### 10.4 What a file contains

| Kind | Examples | Stored in | Used by |
|---|---|---|---|
| Native fields | `x`, `y`, `vx`, `vy`, `sprite`, `color`, `hitbox`, `collides_with`, `tags` | C++ component pools | Movement, collisions, rendering, replication |
| Free fields | `health`, `speed`, `points`, `waypoints` | Lua table of the entity | Scripts only |
| Callbacks | `on_spawn`, `update`, `on_collide`, `on_destroy`, `behavior` | Prefab table | Called by the engine |
| Custom methods | `damage`, `explode` | Prefab table (shared by all instances) | Called with `self:name()` |

Methods are not copied into each instance: they stay in the prefab table, so reloading a file updates every live entity.

Free fields holding a table are the exception: each entity gets its own deep copy at spawn, so `self.waypoints[1] = nil` changes only that entity, never the prefab or the other instances. Nested tables and cycles are copied too (at most 64 levels deep, or the spawn fails); functions and metatables stay shared. Data meant to be shared by every instance, such as a module from `require`, belongs in a `local` of the file, not in a prefab field.

### 10.5 Entity API (`self`)

```lua
self.x, self.y      -- position (native)
self.vx, self.vy    -- velocity: moved by the C++ engine every tick, nil until set
self.age            -- seconds since spawn (read-only)
self.id             -- unique id (read-only)
self.type           -- declared name, e.g. "bydo" (read-only)
self.owner          -- player controlling the entity, or nil

self:destroy()                -- deferred to the end of the tick
self:has_tag("enemy")
self:get_field("PhysicsBody", "mass")      -- any registered component field, nil if absent
self:set_field("PhysicsBody", "mass", 2.5) -- adds the component if needed
self:add_tag("phase2")
self:remove_tag("phase2")
self:distance_to(other)
self:move_to(x, y, duration)  -- inside a coroutine
```

`sprite` is a path relative to `assets/`. `tags` reads back as a new array: change tags by assigning a whole array (`self.tags = { "enemy" }`); entries must be strings and duplicates are dropped. `color` is a table `{ r, g, b, a }` of numbers in [0, 255] (`a` defaults to 255, the default color is white): it tints the sprite, and an entity with a color but no `sprite` is drawn as a square of that color (temporary, until the renderer API lands). It reads back as a new table.

`hitbox = { w, h }` gives the entity a rectangular collider of positive size, anchored at its top-left `x`, `y` (the same corner the sprite is drawn from). `collides_with` is an array of tags: the entity's `on_collide(self, other)` is called on every tick its hitbox overlaps (strictly, touching edges do not count) the hitbox of an entity carrying one of those tags. Both read back as new tables, and both can change at any time, like `tags`. Collisions are one-sided: a missile with `collides_with = { "enemy" }` and an enemy with `collides_with = { "missile" }` each get their own call; an enemy without `collides_with` is never told. Both calls of a pair run even if the first destroys an entity, but an entity destroyed earlier in the tick gets no further collision. Detection runs in C++ (`engine-physics`) after every `update`, so it costs nothing in Lua.

```lua
entity "bydo_shot"

tags          = { "enemy_shot" }
hitbox        = { w = 24, h = 24 }
collides_with = { "player" }

function on_collide(self, other)
    self:destroy()
end
```

`self:destroy()` only marks the entity: it stays usable until the end of the tick, is skipped by later `update` calls, then `on_destroy(self)` runs and the entity is removed. Calling it twice has no extra effect. `on_spawn(self)` runs once the entity and its `spawn` overrides are in place.

`owner` takes a player handle or `nil`, also as a `spawn` override (`spawn("ship", { owner = player })`). The entity then reads that player's input through `self.input`, and clients receive the owner so each one can highlight its own entities. Once the player leaves, `owner` reads `nil`.

Native fields are aliases of component fields declared in the engine's field registry: `x` / `y` are the axes of `Position.pos`, `vx` / `vy` of `PhysicsBody.velocity`, `sprite` is `Sprite.path`, `color` is `Sprite.color`, `tags` is `Tags.labels`, `hitbox` is `AABBCollider.size` and `collides_with` is `CollidesWith.labels`. `get_field` / `set_field` reach every other registered field by component and field name, with the same conversions:

| Field kind | Lua value |
|---|---|
| `float` | finite number |
| `int` | whole number (`4` or `4.0`, not `4.5`) |
| `bool`, `string` | boolean, string |
| `asset` | path string; an empty path reads `nil` |
| `vec2` | `{ x = .., y = .. }` |
| `color` | `{ r, g, b, a }`, numbers in [0, 255], `a` defaults to 255 |
| `list` | array of one kind, e.g. strings; reads an empty array when the component is absent |

Every value read is a new table or a copy: change a field by assigning it. A value of the wrong type, `NaN`, infinity or a value the component rejects raises a Lua error naming the field and the caller's line (`bydo.lua:12: entity field 'hitbox': size must be positive`), and leaves the entity unchanged.

Setting `vx`, `vy` or `hitbox` on an entity without a physics body gives it one: dynamic, without gravity and without collision response. Until then `vx` and `vy` read `nil`, so write `(self.vx or 0)` when an entity may not have moved yet.

**Performance tip:** prefer setting `vx` / `vy` over moving the entity in `update`; native movement runs entirely in C++.

### 10.6 Global functions

```lua
-- entities
spawn("bydo", { x = 1900, y = 300, speed = 200 })   -- optional field overrides
find_all("enemy")          -- entities with a tag
find_first("player")
count("enemy")

-- players
players()                  -- connected players, by ascending id
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

-- utilities
random(1, 10)
world.width, world.height        -- 1920, 1080
log("message")
```

**Sound rules.**

- `sounds` and `musics` take a list of file names and can only be called while the scripts load (top level of any file, or `on_start`). A name is 1 to 63 letters, digits, `_`, `-` or `.`, not starting with `.`. A bad or already declared name raises an error and declares none of the names of that call. R-Type clients look for sounds in `assets/sounds/` and musics in `assets/music/`.
- `play` and `music` with an undeclared name, or with arguments of the wrong type, raise an error. Nothing is played on the server: the requests are queued, and after each tick the server sends them to every client (`sound` and `music` events, see the [protocol](protocol.md)). At most 64 requests are kept per tick.
- The current music is also sent to players who join later.

### 10.7 Systems

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
        spawn("bydo", { x = world.width + 50, y = random(100, 980) })
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

### 10.8 Player input

Clients send **actions**, not keys. The server exposes them on player entities:

```lua
function update(self, dt)
    local input = self.input
    self.vx = input:axis("move_x") * self.speed
    self.vy = input:axis("move_y") * self.speed
    if input:pressed("fire") then
        spawn("player_missile", { x = self.x + 30, y = self.y, owner = self })
    end
end
```

`self.input` is available on every entity and is read-only. `input:axis(name)` returns a value in [-1, 1] (`move_x`: left/right, `move_y`: up/down), `input:pressed(name)` is true while the action is held. An entity reads the input of its `owner`; entities without one read a neutral input (axes 0, nothing pressed).

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

Button actions travel as bits in the order of the shared action list (`fire` is bit 0), axes as `-1`, `0` or `1` (the protocol rounds them); the server and the client use the same table, so the layout cannot drift.

Key bindings are data, editable by the player (remapping, not implemented yet; the defaults below are built in):

```lua
-- config/input.lua
actions = {
    move_up    = { "Up", "Z" },
    move_down  = { "Down", "S" },
    move_left  = { "Left", "Q" },
    move_right = { "Right", "D" },
    fire       = { "Space" },
}
```

### 10.9 Coroutines

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

### 10.10 Networking from the script's point of view

- Scripts are written once; the developer does not write network code.
- All logic (`update`, `on_collide`, `behavior`, systems) runs **on the server only**.
- Clients load the same declarations only to know how each entity type looks (sprite).
- `play` and `music` are sent to clients as events; clients receive the declared names in `connect_accept`.
- Native fields are replicated automatically. A free field is replicated only when marked:

```lua
health = synced(3)   -- visible on clients (e.g. health bar)
speed  = 150         -- server only
```

- Functions and scripts are never sent over the network, only data.

### 10.11 Server tick order

1. Apply received inputs.
2. Run Lua systems.
3. Run entity `update` callbacks and resume coroutines; fire `after` / `every` timers.
4. Native movement (`vx`, `vy`).
5. Collisions (`engine-physics`) → `on_collide` callbacks.
6. Destroy entities marked for destruction → `on_destroy` callbacks.
7. Send snapshot to clients.

### 10.12 Reserved names

Developers must not use these names for their own fields or methods; the engine warns at load time if they do.

- Native fields: `x`, `y`, `vx`, `vy`, `sprite`, `color`, `hitbox`, `collides_with`, `tags`, `layer`, `age`, `id`, `type`, `input`, `owner`.
- Entity methods: `destroy`, `has_tag`, `get_field`, `set_field`.
- Callbacks: `on_spawn`, `update`, `on_collide`, `on_destroy`, `behavior`; in `main.lua`: `on_start`, `on_player_joined`, `on_player_left`.
- Declarations and globals: `entity`, `system`, `query`, `spawn`, `find_all`, `find_first`, `count`, `after`, `every`, `wait`, `play`, `music`, `sounds`, `musics`, `random`, `world`, `log`, `synced`, `players`, `find_player`.

### 10.13 Error handling and sandbox

- Every call into Lua is protected. An error is logged with the file and line (`bydo.lua:14: attempt to index nil value 'target'`), the faulty entity or system is disabled, and the game continues.
- Only safe Lua libraries are opened: `base`, `math`, `string`, `table`, `coroutine`. No `io`, no `os`: scripts cannot access the file system or run commands.
- Common mistake to document: call methods with `self:method()`, not `self.method()`.

### 10.14 How it maps to C++ (for engine developers)

- **Declarations** — every declaration file runs in its own `sol::environment` whose `__index` is the globals table. `entity "name"` / `system "name"` record a pending declaration and return a declarator; calling the declarator with a table (`entity("name")({ ... })`, written `entity "name" { ... }`) stores that table as the prefab. Once the file has run, an uncalled declarator (short form) takes the file environment as its prefab. A file with one short form, or only table forms, is registered as a whole; two short forms, or a short form mixed with table forms, rejects the file. The registry maps each name to its prefab table, kind and source path.
- **Global functions** are C++ lambdas exposed with sol2:

  ```cpp
  lua.set_function("spawn", [&](const std::string& type, sol::optional<sol::table> overrides) {
      return world.spawn(type, overrides);
  });
  ```

- **Field access** goes through the entity handle's metatable. `__index` looks up, in order: native field aliases → entity methods → the entity's Lua data → its prefab table (methods). `__newindex` writes native fields into C++ storage and everything else into the Lua data. Aliases are a small data table (`engine/src/script/EntityFields.cpp`) mapping a Lua name to a `core::FieldRegistry` field, resolved to a `FieldRef` once; `sol::object` ↔ `FieldValue` conversion lives in one place (`LuaFieldValue.cpp`). The registry, filled by `register_core_fields` and `register_physics_fields`, is exposed by `ScriptRuntime::fields()`. Setting a collider field also adds the `PhysicsBody` it requires (`FieldRegistry::add(..., required<PhysicsBody>)`); the `hitbox` alias additionally makes a new body dynamic without gravity or collision response, centres the collider (`offset = size / 2`) and adds `Scriptable`.
- **Callbacks** are called by native systems through `sol::protected_function`.
- **Coroutines** are `sol::thread` + `sol::coroutine`; `wait(t)` yields and the scheduler resumes the coroutine when `t` has elapsed.

---

## 11. Engine rules and conventions

- Nothing in `engine/` knows about R-Type. Test for every addition: *would this make sense in a platformer?*
- Public engine symbols are marked with the export macro generated by CMake; default visibility is hidden on every platform.
- No global state in headers (no `static` variables in templates): registries and singletons live in a `.cpp` of `engine-core`, behind exported functions. Component identity uses stable names, not per-module counters.
- Third-party headers are never included in public engine headers.
- Resources are referenced by name (`"bydo.png"`); the resource manager loads and caches them.
- Every technical choice is written down with its justification in the [comparative study](comparative-study.md).

---

## 12. Accessibility

Accessibility is designed in from Part 1, not added at the end.

| Category | Measures |
|---|---|
| Motor | Fully remappable controls (input actions); adjustable game speed; no action requiring rapid repeated presses (hold-to-fire). |
| Visual | Players distinguishable by shape, number and pattern, not only colour; colour-blind friendly palette; adjustable text size; high-contrast mode. |
| Auditory | Separate music / effects volumes; visual cues for important sounds (incoming enemy, damage). |
| Cognitive | Pause at any time; simple menus; adjustable difficulty; clear on-screen feedback. |

What is implemented and what is planned, measure by measure, is in [Accessibility](accessibility.md).

---

## 13. Quality: tests, CI, packaging

- **Tests (GoogleTest)**, run on every pull request. Priorities:
  1. protocol parsing: truncated packets, wrong sizes, unknown types, random bytes;
  2. ECS behaviour;
  3. Lua loading: duplicate names, scripts with errors, reserved names;
  4. plugin loading: missing library, missing symbol, wrong API version.
- **CI:** Linux (GCC, Clang) and macOS builds with cached dependencies, AddressSanitizer + UBSan, ThreadSanitizer, clang-tidy and a format check on every pull request.
- **Packaging:** `cmake --install` produces a self-contained `bin/` folder with the executables, engine libraries, `plugins/`, scripts and assets. CPack archives on top of it are planned.

---

## 14. Documentation

All documentation is in English and published online.

| Document | Where |
|---|---|
| README: description, dependencies, build, run, quick start | [README](https://github.com/Saitamadupauvre/RType#readme) |
| Getting started and how to play | [getting-started.md](getting-started.md), [how-to-play.md](how-to-play.md) |
| Architecture diagrams, modules, engine rules | [architecture.md](architecture.md), [engine.md](engine.md), [networking.md](networking.md), [plugins.md](plugins.md) |
| How-to guides (add an enemy, tune the game) | [writing-the-game.md](writing-the-game.md) |
| Lua API reference | §10 of this page |
| Protocol specification (RFC-style) | [protocol.md](protocol.md) |
| Comparative study (technologies, algorithms, storage, security) | [comparative-study.md](comparative-study.md) |
| Accessibility report | [accessibility.md](accessibility.md) |
| Contribution guide and commands | [contributing.md](contributing.md), [commands.md](commands.md) |

The site is built with MkDocs from `docs/` and published to GitHub Pages on every push to `main`, with the Doxygen API reference under `/api/`.

---

## 15. Roadmap

### Part 1 (weeks 1–3)

| Week | Goals |
|---|---|
| 1 | ECS core and fixed-timestep loop; graphics plugin (window, starfield, sprites, input); network foundations; Lua runtime loading declarations; CI green on Linux and macOS. |
| 2 | Lua API: entities, callbacks, globals, systems; player movement and shooting; Bydo spawning; collisions; replication of native fields; sounds. |
| 3 | 4-player stability, disconnection handling, malformed-packet tests, accessibility measures, documentation, packaging, tag `v1.0`. |

Internal milestones: end of week 1 — a square moving in sync on two clients; end of week 2 — scripted Bydos, shooting and collisions over the network.

Part 1 status:

| Goal | Status |
|---|---|
| ECS, fixed-timestep loop, event bus, scheduler | Done |
| Graphics plugin: window, sprites, input, letterboxing | Done |
| Starfield | Done |
| Network: protocol, threads, sessions, timeouts, snapshots, statistics | Done |
| Lua runtime: declarations, entities, players, sandbox | Done |
| Player movement and shooting, Bydo spawning, game over | Done (in Lua) |
| Collisions | Done (`engine-physics` in the server tick, `on_collide` in Lua) |
| Up to 4 players, distinguishable, crash notification | Done |
| Sound: audio plugin, Lua `play` / `music`, replication, volumes | Done |
| Enemy missiles | Done |
| Accessibility settings | To do, see [Accessibility](accessibility.md) |
| Documentation | Done |
| Tag `v1.0` | To do |

### Part 2 (weeks 4–6) — Advanced Game Engine

1. **Modularity:** finer-grained libraries; more subsystems as runtime plugins.
2. **Standalone engine:** `engine/` moves to its own repository; R-Type consumes it as a dependency.
3. **Generic runtime:** the executables' `main` functions become generic `ServerApp` / `ClientApp` in the engine, with a launcher (`engine run <game-folder>`); `r-type_server` / `r-type_client` remain as thin entry points.
4. **Tooling:** hot reload of scripts, developer console executing Lua, level editor.
5. **Second game:** a different game (e.g. a platformer or networked Breakout) written only in Lua and assets, proving the engine is generic.

---

## 16. Open decisions

| Topic | Question | Proposal |
|---|---|---|
| Package manager | vcpkg or CPM? | **Decided:** vcpkg (manifest mode) |
| Native fields | Final list | `x`, `y`, `vx`, `vy`, `sprite`, `hitbox`, `collides_with`, `tags`, maybe `animation` |
| Naming | `update` or `on_update`; `after`/`every` or a `timer` object | `update`, `after`/`every` |
| Destruction | Immediate or deferred? | **Decided:** deferred to the end of the tick |
| Levels | System, or dedicated `level` declaration with a `behavior` coroutine | Dedicated declaration |
| Game flow | Lobby / ready button, join in progress, respawn, more than 4 players | **Decided for Part 1:** no lobby, join in progress, no respawn, restart after game over, 4 players maximum |
| Tick / snapshot rates | Snapshot every tick or less often | **Decided for Part 1:** 60 Hz tick, one snapshot per tick |
| Second game | Which one? | Choose early to validate abstractions |
