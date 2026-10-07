# R-Type — Project Overview

> Networked multiplayer shoot'em up built on our own C++ game engine, with gameplay written in Lua.
> Target track for Part 2: **Track 1 — Advanced Game Engine**.

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

### Part 2 — Advanced Game Engine (week 6)

See [Roadmap](#15-roadmap). Main goals: stronger modularity, engine extracted as a standalone project, generic runtime, tooling (level editor, developer console, hot reload), and a second game.

### Common requirements (evaluated at both defenses)

- Cross-platform: Linux (required) and macOS, tested in CI from day one. Windows support is postponed: new code is not required to compile on Windows for now, and the CI has no Windows job.
- Dependencies handled only by a package manager; no library sources copied into the repository.
- Git workflow: branches, pull requests, reviews, issues, tags.
- Documentation in English, published online.
- Accessibility solutions (motor, visual, auditory, cognitive).

---

## 3. Technology stack

| Area | Choice | Why |
|---|---|---|
| Language | C++20 | Required by the subject; concepts, designated initializers, `std::span`. |
| Build system | CMake | Standard, cross-platform, integrates with package managers. |
| Package manager | vcpkg (manifest mode) — *to confirm* | All our dependencies are ports; binary caching for CI; copies DLLs automatically on Windows. |
| Windowing / 2D rendering / input / audio | Raylib | One small library for all four; simple API. Isolated in a plugin. |
| Networking | Asio (standalone) | Portable async UDP on Linux and Windows. |
| Scripting | Lua 5.4 + sol2 | Lightweight, designed to be embedded, industry standard for game scripting; sol2 gives a modern C++ binding. |
| Tests | GoogleTest | Decided. |
| Logging | spdlog | Levels, timestamps, per-module loggers. |
| Documentation | MkDocs (or equivalent) | Markdown in the repository, published online. |
| Formatting / CI | Already set up by the team | clang-format, Linux + macOS builds. |

Linux system packages required by Raylib (X11, OpenGL, ALSA development headers) are low-level system dependencies and are listed in the README.

---

## 4. Architecture

### 4.1 Overview

```
            ┌───────────────────────── Engine (C++) ─────────────────────────┐
            │                                                                │
            │  engine-core     ECS, game loop, time, events, resources,      │
            │                  logging, input actions, plugin loader         │
            │  engine-script   Lua runtime (sol2), entity/system loading     │
            │  engine-network  Asio, protocol, replication                   │
            │                                                                │
            │  plugins/graphics-raylib   window, 2D rendering, input, audio  │
            │                            (loaded at runtime, client only)    │
            └────────────────────────────────────────────────────────────────┘
                        │                                      │
             core + script + network            core + script + network
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
- **Third-party headers stay private.** `raylib.h` is included only inside the graphics plugin; Asio only inside `.cpp` files of the network module. This also avoids the well-known `windows.h` / Raylib name clashes on Windows.
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
│   ├── include/engine/     public headers (core, graphics interfaces, platform, script, network)
│   └── src/                implementation
├── plugins/
│   └── graphics-raylib/    the only code that includes raylib.h
├── game/
│   ├── server/main.cpp     → r-type_server
│   ├── client/main.cpp     → r-type_client
│   └── scripts/            Lua game code (free layout, see §10)
├── assets/                 sprites, sounds, music
├── tests/                  GoogleTest suites
├── docs/                   MkDocs sources
├── vcpkg.json
└── CMakeLists.txt
```

### Build output

```
bin/
├── r-type_server
├── r-type_client
├── libengine-core.so
├── libengine-script.so
├── libengine-network.so
└── plugins/
    └── graphics-raylib.so
```

Executables locate their libraries, plugins, scripts and assets **relative to their own location** (RPATH `$ORIGIN` on Linux, same folder on Windows), never relative to the current working directory, so the game still works once installed.

---

## 6. Executables

In Part 1, each executable has a short, explicit `main` that assembles engine modules. Nothing is hidden: reading the `main` shows the whole frame or tick.

```cpp
// game/server/main.cpp (simplified)
int main(int argc, char** argv)
{
    engine::World world;
    engine::LuaRuntime lua(world);
    engine::NetworkServer net(parse_port(argc, argv));   // runs its own network thread

    lua.load_game("scripts");

    engine::FixedLoop loop(60);
    loop.run([&](float dt) {
        net.receive_inputs(world);   // drains the thread-safe queue
        lua.tick(dt);                // systems, entity updates, coroutines
        engine::physics::step(world, dt);
        net.send_snapshot(world);
    });
}
```

```cpp
// game/client/main.cpp (simplified)
int main(int argc, char** argv)
{
    engine::graphics::GraphicsPlugin gfx("plugins/graphics-raylib");
    engine::World world;
    engine::LuaRuntime lua(world);
    engine::NetworkClient net(argv[1], parse_port(argc, argv));

    lua.load_game("scripts");   // only to know how each entity type looks

    gfx->renderer().open({ .width = 1280, .height = 720, .title = "R-Type" });
    while (!gfx->renderer().should_close()) {
        net.send_inputs(input_map.read(gfx->input()));
        net.apply_snapshots(world);
        render(world, gfx->renderer());
    }
}
```

Usage:

```
./r-type_server <port>
./r-type_client <server-ip> <port>
```

---

## 7. Server model

- **Authoritative:** clients send **inputs** (actions), never positions. The server simulates and replicates the result. This prevents trivial cheating.
- **Multithreaded:**
  - a **network thread** (Asio) receives packets, validates them and pushes messages into a thread-safe queue;
  - the **game thread** drains the queue at each tick, runs the simulation and sends snapshots.
  The game loop never blocks waiting for a client.
- **Resilient:**
  - a client that stops sending heartbeats is disconnected after a timeout and the other players are notified;
  - a malformed packet is dropped and logged, never trusted;
  - a Lua error is caught, logged, and disables the faulty entity or system without stopping the game.

### Game flow (Part 1, minimal)

```
Waiting for players ──(start)──▶ Playing ──(all players dead)──▶ Game over ──▶ Waiting
```

Details (lobby, join in progress, respawn rules, maximum 4 players) are listed in [Open decisions](#16-open-decisions).

---

## 8. Networking

The protocol is designed and implemented by a dedicated team member and documented separately (RFC-style). This section only lists the constraints the rest of the engine relies on:

- UDP for all gameplay traffic, binary encoding with a fixed endianness.
- Common packet header (at least: magic number, protocol version, message type, sequence number, payload size).
- Packets kept small enough to avoid IP fragmentation.
- Messages for at least: connection handshake, inputs, snapshots, entity spawn/destroy, sound events, heartbeat, disconnection.
- Entity types are sent as numeric IDs; the table *type name → ID* is sent at connection time.
- Every incoming packet is validated (size, type, bounds) before use.

The engine replicates **native fields** automatically and any Lua field marked `synced(...)` (see §10.8).

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
    // ...
};

class IAudio {   // separate music / effects volumes
    virtual SoundId load_sound(const std::string& path) = 0;
    virtual void play_sound(SoundId, float volume = 1.f, float pitch = 1.f) = 0;
    virtual MusicId load_music(const std::string& path) = 0;
    virtual void play_music(MusicId, bool loop = true) = 0;
    virtual void set_music_volume(float) = 0;
    virtual void set_effects_volume(float) = 0;
    // ...
};

class IGraphicsBackend {   // what a plugin provides
    virtual IRenderer& renderer() = 0;
    virtual IInput& input() = 0;
    virtual IAudio& audio() = 0;
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

---

## 10. Lua scripting API

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
2. Call `on_start()` from `main.lua`.

Since `spawn` is only used once the game has started, file order never matters.

### 10.3 Declaring an entity

**One entity per file** (default style):

```lua
-- stage1/bydo.lua
entity "bydo"

-- native fields: handled by the C++ engine
sprite = "bydo.png"
hitbox = { w = 32, h = 32 }
tags   = { "enemy" }

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

function on_hit(self, other)
    if other:has_tag("player_missile") then self:damage(1) end
end

-- custom methods: as many as needed
function damage(self, amount)
    self.health = self.health - amount
    if self.health <= 0 then self:explode() end
end

function explode(self)
    spawn("explosion", { x = self.x, y = self.y })
    play("explosion.wav")
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
| Native fields | `x`, `y`, `vx`, `vy`, `sprite`, `hitbox`, `tags` | C++ component pools | Movement, collisions, rendering, replication |
| Free fields | `health`, `speed`, `points` | Lua table of the entity | Scripts only |
| Callbacks | `on_spawn`, `update`, `on_hit`, `on_destroy`, `behavior` | Prefab table | Called by the engine |
| Custom methods | `damage`, `explode` | Prefab table (shared by all instances) | Called with `self:name()` |

Methods are not copied into each instance: they stay in the prefab table, so reloading a file updates every live entity.

### 10.5 Entity API (`self`)

```lua
self.x, self.y      -- position (native)
self.vx, self.vy    -- velocity: moved by the C++ engine every tick
self.age            -- seconds since spawn (read-only)
self.id             -- unique id (read-only)
self.type           -- declared name, e.g. "bydo" (read-only)

self:destroy()                -- deferred to the end of the tick
self:has_tag("enemy")
self:add_tag("phase2")
self:remove_tag("phase2")
self:distance_to(other)
self:move_to(x, y, duration)  -- inside a coroutine
```

**Performance tip:** prefer setting `vx` / `vy` over moving the entity in `update`; native movement runs entirely in C++.

### 10.6 Global functions

```lua
-- entities
spawn("bydo", { x = 1900, y = 300, speed = 200 })   -- optional field overrides
find_all("enemy")          -- entities with a tag
find_first("player")
count("enemy")

-- time
after(2, function() ... end)     -- run once after 2 s
every(0.5, function() ... end)   -- run every 0.5 s (returns a handle to cancel it)
wait(1.5)                        -- only inside a coroutine

-- sound (replicated to clients)
play("shot.wav")
music("stage1.ogg")

-- utilities
random(1, 10)
world.width, world.height        -- 1920, 1080
log("message")
```

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
    e.vy = e.vy + e.gravity * dt
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

Key bindings are data, editable by the player (remapping):

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
- All logic (`update`, `on_hit`, `behavior`, systems) runs **on the server only**.
- Clients load the same declarations only to know how each entity type looks (sprite, sounds).
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
5. Collisions → `on_hit` callbacks.
6. Destroy entities marked for destruction → `on_destroy` callbacks.
7. Send snapshot to clients.

### 10.12 Reserved names

Developers must not use these names for their own fields or methods; the engine warns at load time if they do.

- Native fields: `x`, `y`, `vx`, `vy`, `sprite`, `hitbox`, `tags`, `layer`, `age`, `id`, `type`, `input`.
- Callbacks: `on_spawn`, `update`, `on_hit`, `on_destroy`, `behavior`.
- Declarations and globals: `entity`, `system`, `query`, `spawn`, `find_all`, `find_first`, `count`, `after`, `every`, `wait`, `play`, `music`, `random`, `world`, `log`, `synced`.

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

- **Field access** goes through the entity handle's metatable. `__index` looks up, in order: native components → the entity's Lua data → its prefab table (methods). `__newindex` writes native fields into C++ storage and everything else into the Lua data.
- **Callbacks** are called by native systems through `sol::protected_function`.
- **Coroutines** are `sol::thread` + `sol::coroutine`; `wait(t)` yields and the scheduler resumes the coroutine when `t` has elapsed.

---

## 11. Engine rules and conventions

- Nothing in `engine/` knows about R-Type. Test for every addition: *would this make sense in a platformer?*
- Public engine symbols are marked with the export macro generated by CMake; default visibility is hidden on every platform.
- No global state in headers (no `static` variables in templates): registries and singletons live in a `.cpp` of `engine-core`, behind exported functions. Component identity uses stable names, not per-module counters.
- Third-party headers are never included in public engine headers.
- Resources are referenced by name (`"bydo.png"`); the resource manager loads and caches them.
- Every technical choice is written down in `docs/decisions/` with its justification (feeds the comparative study).

---

## 12. Accessibility

Accessibility is designed in from Part 1, not added at the end.

| Category | Measures |
|---|---|
| Motor | Fully remappable controls (input actions); adjustable game speed; no action requiring rapid repeated presses (hold-to-fire). |
| Visual | Players distinguishable by shape, number and pattern, not only colour; colour-blind friendly palette; adjustable text size; high-contrast mode. |
| Auditory | Separate music / effects volumes; visual cues for important sounds (incoming enemy, damage). |
| Cognitive | Pause at any time; simple menus; adjustable difficulty; clear on-screen feedback. |

Each measure is documented as it is implemented.

---

## 13. Quality: tests, CI, packaging

- **Tests (GoogleTest)**, run on every pull request. Priorities:
  1. protocol parsing: truncated packets, wrong sizes, unknown types, random bytes;
  2. ECS behaviour;
  3. Lua loading: duplicate names, scripts with errors, reserved names;
  4. plugin loading: missing library, missing symbol, wrong API version.
- **CI:** Linux and macOS builds with cached dependencies (already set up).
- **Packaging:** CPack produces archives/installers containing executables, engine libraries, the `plugins/` folder, scripts and assets.

---

## 14. Documentation

All documentation is in English and published online.

- **README:** project description, dependencies, build and run instructions, quick start.
- **Developer documentation:** architecture diagrams, modules, engine rules, how-to guides (create an entity, a system, a level).
- **Lua API reference:** derived from §10.
- **Protocol specification:** RFC-style, maintained by the protocol owner.
- **Comparative study:** justification of each technology (C++20, CMake + vcpkg, Raylib, Asio, Lua 5.4 + sol2 vs alternatives such as LuaJIT or Python), including algorithms, data storage and security aspects.
- **Accessibility report.**

---

## 15. Roadmap

### Part 1 (weeks 1–3)

| Week | Goals |
|---|---|
| 1 | ECS core and fixed-timestep loop; graphics plugin (window, starfield, sprites, input); network foundations; Lua runtime loading declarations; CI green on Linux and macOS. |
| 2 | Lua API: entities, callbacks, globals, systems; player movement and shooting; Bydo spawning; collisions; replication of native fields; sounds. |
| 3 | 4-player stability, disconnection handling, malformed-packet tests, accessibility measures, documentation, packaging, tag `v1.0`. |

Internal milestones: end of week 1 — a square moving in sync on two clients; end of week 2 — scripted Bydos, shooting and collisions over the network.

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
| Package manager | vcpkg or CPM? | vcpkg (manifest mode) |
| Native fields | Final list | `x`, `y`, `vx`, `vy`, `sprite`, `hitbox`, `tags`, `layer`, maybe `animation` |
| Naming | `update` or `on_update`; `after`/`every` or a `timer` object | `update`, `after`/`every` |
| Destruction | Immediate or deferred? | Deferred to the end of the tick |
| Levels | System, or dedicated `level` declaration with a `behavior` coroutine | Dedicated declaration |
| Game flow | Lobby / ready button, join in progress, respawn, more than 4 players | Minimal flow in Part 1 |
| Tick / snapshot rates | Snapshot every tick or less often | 60 Hz tick; snapshot rate decided with the protocol owner |
| Second game | Which one? | Choose early to validate abstractions |
