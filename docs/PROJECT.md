# Project overview

> A 2D multiplayer game engine in C++, with an editor to build games and a runtime to play and ship them. Games are written in Lua; R-Type is the example game.
> Target track for Part 2: **Track 1 — Advanced Game Engine**.

This page is the reference for the vision, the architecture decisions and the roadmap. It describes the **target design**; where the implementation is not there yet, the section says so. For the current state of the code, start with [Architecture](architecture.md). The Lua API has its own page: [Lua API reference](reference/lua-api.md).

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

We are building a **reusable 2D multiplayer game engine** and the **editor** to make games with it. R-Type, the shoot'em up of the Epitech subject, is one example of what the editor makes.

- The **engine** is written in C++. It knows nothing about any game: it provides an ECS, a game loop, networking, rendering, audio, input and a Lua runtime.
- The **editor** opens a game folder, lays out its scenes, edits the fields of every placed entity, plays the game in one click and exports it.
- The **runtime** (`engine-runtime`) runs any game folder: alone, hosted, joined over the network or as a dedicated server. An exported game is the runtime renamed after the game, next to the game's files.
- A **game** is a folder of data: `game.json`, Lua scripts, JSON scenes and assets. Nothing in it is compiled.
- The server is **authoritative**: all game logic runs on the server. Clients send inputs and display what the server replicates.

The success criterion is simple: **a different game can be built by writing only Lua scripts and assets, and laid out in the editor**.

---

## 2. Scope and deliverables

### Part 1 — Prototype (week 3)

A playable, networked R-Type prototype:

- Starfield scrolling background, timer-based (never tied to CPU speed): a Lua `space` entity drawn through `assets/shaders/space.fs` ([`shader`](reference/lua-api.md#entity-api-self)).
- Up to 4 players, visually distinguishable (not only by colour).
- Players move with the arrow keys and shoot.
- Bydos (enemies) spawn randomly from the right side of the screen.
- Missiles, collisions, destruction.
- Some sound effects.
- Multithreaded authoritative server; survives client crashes and notifies other players.
- Binary UDP protocol, robust to malformed packets, documented RFC-style.
- Engine split into decoupled subsystems (rendering, networking, logic), built as shared libraries, with the graphics backend loaded as a runtime plugin.
- Gameplay written in Lua on top of the engine.

Part 1 is delivered by the R-Type example game, `examples/r-type/`.

Progress is tracked in the [roadmap](#15-roadmap).

### Part 2 — Advanced Game Engine (week 6)

See [Roadmap](#15-roadmap). Main goals: the editor as the product, stronger modularity, engine extracted as a standalone project, generic runtime, tooling (developer console, hot reload), and a second game.

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
| Scene and project files | JSON through nlohmann-json | Header-only and standard; keeps key order, so saved scenes stay stable in diffs. Private to `engine-script`. |
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
            │  engine-editor   editing model: scenes, commands, undo/redo    │
            │                                                                │
            │  plugins/graphics-raylib   window, 2D rendering, input         │
            │                            (loaded at runtime, client only)    │
            └────────────────────────────────────────────────────────────────┘
                    │                                   │
        engine-editor + Clay + raylib          engine-app (ServerApp, ClientApp)
                    ▼                                   ▼
                 editor ───── Play / Export ─────▶ engine-runtime
                    │                                   │
                    └────── game folder (game.json, scripts/, scenes/, assets/) ──┘
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

```
<repository>/
├── engine/
│   ├── cmake/              engine_add_module / engine_add_plugin
│   ├── include/engine/     public headers (core, graphics, audio, network, physics, script, app, editor)
│   └── src/                implementation
├── plugins/
│   ├── graphics-raylib/    the only engine code that includes raylib.h
│   └── audio-raylib/
├── editor/                 the editor app (Clay layout, raylib only in src/raylib/)
├── runtime/                engine-runtime: runs any game folder in every mode
├── examples/
│   └── r-type/             example game: game.json, scripts/, scenes/, assets/
├── ports/                  vcpkg overlay ports (clap)
├── tests/                  GoogleTest suites mirroring engine/, plugins/, editor/, runtime/
├── docs/                   MkDocs sources
├── mkdocs.yml
├── vcpkg.json
└── CMakeLists.txt
```

### Build output

```
bin/
├── editor
├── editor-assets/fonts/
├── engine-runtime
├── libengine-{app,audio,core,editor,graphics,navigation,network,physics,script}.so
└── plugins/
    ├── audio-raylib.so
    └── graphics-raylib.so
```

Executables locate their libraries and plugins **relative to their own location** (RPATH `$ORIGIN` on Linux), never relative to the current working directory. Games are not copied into `bin/`: the editor and the runtime take the game folder as an argument. An exported game puts the runtime and the game folder side by side, and the runtime then uses its own folder as the game folder.

---

## 6. Executables

There are two executables. `editor` edits a game folder (see [The editor](guide/editor-tour.md)). `engine-runtime` plays one: its `main` parses the command line and calls `engine::app::launch()` with a mode. `engine-app` holds a generic `ServerApp` and `ClientApp` that run any game folder (see [Game folders and scenes](guide/game-folder.md)) and know nothing about any game.

| Mode | Server | Client | Network |
|---|---|---|---|
| `dedicated` | `ServerApp` on the main thread | none | UDP |
| `client` | elsewhere | `ClientApp` | UDP |
| `host` | `ServerApp` on a background thread | `ClientApp` on `127.0.0.1` | UDP, others can join |
| `solo` | `ServerApp` and `ClientApp` in the same thread | | `LoopbackNetwork`, no socket |

`engine-runtime` offers every mode, and so does an exported game, which is the same binary renamed. Simplified from the real code:

```cpp
// engine/src/app/ServerApp.cpp: one tick, run at 60 Hz by a FixedStepLoop
void ServerApp::Impl::tick(Duration step)
{
    network.receive();                     // sessions, timeouts, latest input of each player
    apply_inputs();                        // to the Lua entities each player owns
    runtime.update(dt);                    // every entity's update(self, dt), then main.lua update(dt)
    collisions.step(dt);                   // engine-physics, then on_collide
    runtime.flush_destroyed();             // on_destroy, then removal
    runtime.flush_scene();                 // load_scene: swap non-persistent entities
    systems.run(world, dt);                // native systems
    replicate();                           // mark scripted entities with a sprite or colour
    publish_audio(); publish_events();     // play, music, emit
    network.send_snapshot(world);          // queued for the send thread
}
```

```cpp
// engine/src/app/ClientApp.cpp: one frame
void ClientApp::Impl::frame()
{
    network.update();                                  // snapshots, events, retries, heartbeats
    audio.update();
    const bool playing = assets_ready();               // preloads 4 assets per frame (§9.5)
    if (playing) {
        run_ticks();                                   // fixed 60 Hz: Lua `input`, update(dt), send input
    }
    sound.apply(client.audio_settings());              // Lua `audio`
    draw(playing);                                     // the game, or the loading screen
}
```

Usage:

```
engine-runtime --game <folder> --server <port> [--max-players <1-4>]
engine-runtime --game <folder> <server-ip> <port>
engine-runtime --game <folder> --host <port>
engine-runtime --game <folder> --solo
```

Every option is in [Run and export](guide/run-and-export.md).

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

In the R-Type example, implemented in `examples/r-type/scripts/game.lua`: the first player to join starts the round, later players get a ship at once, and the round restarts 3 seconds after the last ship is destroyed. Threads, queues and failure handling are detailed in [Networking internals](networking.md).

---

## 8. Networking

The protocol is specified RFC-style in [protocol.md](protocol.md) and its implementation is described in [Networking internals](networking.md). This section only lists the constraints the rest of the engine relies on:

- UDP for all gameplay traffic, binary encoding with a fixed endianness.
- Common packet header (at least: magic number, protocol version, message type, sequence number, payload size).
- Packets kept small enough to avoid IP fragmentation.
- Messages for at least: connection handshake, inputs, snapshots, entity spawn/destroy, sound events, heartbeat, disconnection.
- Entity types are sent as numeric IDs; the table *type name → ID* is sent at connection time.
- Every incoming packet is validated (size, type, bounds) before use.

The engine replicates the type of every replicated entity and each component field flagged `replicated` in the field registry it owns: for example position, velocity, owner and colour. Snapshot entities are `entity_id, type_id, [field_id, value]*`, field names are sent once in `connect_accept`, so a new replicated field needs no protocol change. Replicating Lua fields marked `synced(...)` is planned (see the [Lua API](reference/lua-api.md#networking-from-the-scripts-point-of-view)).

---

## 9. Graphics plugin

### 9.1 Interfaces

The engine only knows these interfaces (in `engine/include/engine/graphics/`). They use engine types (`Vec2`, `Rect`, `Color`, `TextureId`, `RenderTargetId`, `ShaderId`...) and never Raylib types.

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
    // render targets: off-screen surfaces, drawn back like any texture
    virtual RenderTargetId create_render_target(Vec2 size) = 0;   // pixels
    virtual void destroy_render_target(RenderTargetId) = 0;
    virtual void begin_target(RenderTargetId, Color clear) = 0;
    virtual void end_target() = 0;
    virtual TextureId target_texture(RenderTargetId) const = 0;
    // shaders (called through the engine's shader library, see 9.5)
    virtual ShaderId load_shader(const std::string& vertex, const std::string& fragment) = 0;
    virtual void unload_shader(ShaderId) = 0;
    virtual void begin_shader(ShaderId) = 0;
    virtual void end_shader() = 0;
    virtual void set_uniform(ShaderId, const std::string& name, float) = 0;
    virtual void set_uniform(ShaderId, const std::string& name, Vec2) = 0;
    virtual void set_uniform(ShaderId, const std::string& name, Color) = 0;      // vec4 in [0, 1]
    virtual void set_uniform(ShaderId, const std::string& name, TextureId) = 0;  // sampler2D
    // frame
    virtual void begin_frame(Color clear) = 0;
    virtual void end_frame() = 0;
    // world space: draw calls in between take world units, seen through a camera view
    virtual void begin_world(const CameraView&) = 0;
    virtual void end_world() = 0;
    Vec2 world_to_screen(const CameraView&, Vec2 world) const;   // non-virtual helpers
    Vec2 screen_to_world(const CameraView&, Vec2 screen) const;
    // drawing, in virtual units (screen space outside begin_world / end_world)
    virtual void draw_sprite(const SpriteDraw&) = 0;
    virtual void draw_rect(const Rect&, Color, bool filled = true) = 0;
    virtual void draw_circle(Vec2 center, float radius, Color) = 0;
    virtual void draw_line(Vec2 from, Vec2 to, float thickness, Color) = 0;
    virtual void draw_text(const std::string&, Vec2 pos, float size, Color) = 0;
    virtual Vec2 measure_text(const std::string&, float size) const = 0;
};

struct SpriteDraw {
    TextureId texture;
    Rect source;          // texture pixels; a negative width or height mirrors the sprite
    Rect dest;            // dest.x, dest.y is where origin lands
    Vec2 origin;          // pivot, relative to the top-left of the drawn rectangle
    float rotation = 0;   // degrees around origin, clockwise on screen
    Color tint;
};

class IInput {   // raw keys; the engine's InputMap turns them into actions
    virtual bool is_key_down(Key) const = 0;
    virtual bool is_key_pressed(Key) const = 0;
    virtual bool is_key_released(Key) const = 0;
    virtual Key  last_key_pressed() = 0;   // for the key-remapping screen
    virtual char32_t next_character() = 0;  // typed text, for the developer console
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
- **World space and screen space.** Between `begin_world(view)` and `end_world()`, draw calls take world units: a `CameraView` (`center`, the world point shown at the middle of the screen, and `zoom`) turns them into virtual units, `screen = (world - center) * zoom + virtual_size / 2`. Everything else, such as a HUD, is drawn in screen space and does not move with the camera. `end_frame()` ends world space if needed. The client draws the replicated entities in world space every frame.
- **Render targets.** `create_render_target(size)` makes an off-screen surface of `size` pixels. Between `begin_target(target, clear)` and `end_target()`, draw calls go into it instead of the frame: screen space is the target's own pixels, and `begin_world(view)` centers the view on the target. `target_texture(target)` is a normal `TextureId`, drawn upright with `draw_sprite` (the backend handles the flip of the render texture); `unload_texture` ignores it, it lives as long as its target. `begin_target` and `end_target` end world space first, so the editor viewport is `begin_target` → `begin_world` → draw → `end_world` → `end_target` → `draw_sprite(target_texture)`. Calling `begin_target` again switches target; targets do not nest.
- **Shaders.** `load_shader(vertex, fragment)` compiles GLSL files; an empty path uses the backend's default stage. Between `begin_shader(shader)` and `end_shader()`, draw calls go through it; calling `begin_shader` again replaces it. Uniforms keep their value across frames and only affect the draw calls made after them; unknown shaders and uniforms the program does not use are ignored. A missing file, a compile or link error gives the default `ShaderId`, is logged, and `begin_shader` on it draws with the default pipeline, so drawing always continues.
- **Lifetime.** `destroy_render_target` / `unload_shader` end the target or shader first if it is current; `end_frame()` ends the current target, world space and shader; `close()` frees every texture, target and shader.
- **Camera.** The view comes from, in this order: the local override of the client scripts (`camera.*`, see the [Lua API](reference/lua-api.md#networking-from-the-scripts-point-of-view)), the scene camera (the replicated entity with a `Camera` component and the lowest id, clamped to its bounds), or the default view (`center = virtual_size / 2`, `zoom = 1`), which draws exactly as without a camera. Camera entities are never drawn. The conversion math lives in `engine-core` (`engine/core/math/Camera.hpp`), so the script module uses it without depending on graphics.

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

### 9.5 Asset cache

The game refers to assets by name; `engine::core::AssetCache<Id>` (`engine/core/util/AssetCache.hpp`) turns a name into a backend handle. It is built with a folder and a load / unload pair, e.g. `IRenderer::load_texture` / `unload_texture`.

```cpp
core::AssetCache<graphics::TextureId> textures(assets, load, unload);
graphics::TextureId ship = textures.get("sprites/ship.png");   // loads assets/sprites/ship.png once
```

- `get(name)` loads a name the first time and returns the same handle afterwards.
- A name is a relative path of `/`-separated segments; each segment is non-empty, does not start with `.`, and only holds letters, digits, `_`, `-` and `.`. At most 255 bytes (`is_valid_asset_name`). Absolute paths, `..`, backslashes and hidden files are rejected without touching the backend.
- A rejected name or a failed load gives the placeholder (default id), is logged once and never retried.
- `clear()` and the destructor unload every loaded handle; the backend must outlive the cache.
- Main thread only, like the backends. The client uses one cache for textures (`assets/`), one for sounds (`assets/sounds/`) and one for musics (`assets/music/`).
- Entity shaders (`shader` field, see the [Lua API](reference/lua-api.md#entity-api-self)) are drawn by `WorldDraw`, which sets the `time` and `resolution` uniforms, then `begin_shader` / draw / `end_shader` around the entity. Shader names never travel as text: the server lists them in the `shader` category of the string table, `Shader.path` is an asset field of type `AssetType::Shader`, and the client preloads them with the textures.
- Shaders go through `engine::app::ShaderLibrary`, a cache of `ShaderId` over `IRenderer::load_shader` / `unload_shader`. A shader is named without extension: `shaders/wave` loads `assets/shaders/wave.fs` (required) and `assets/shaders/wave.vs` when it exists, the default vertex stage otherwise. A missing or invalid shader is reported once and gives the default `ShaderId`, which draws without a shader.

```cpp
app::ShaderLibrary shaders(renderer, assets);
graphics::ShaderId wave = shaders.get("shaders/wave");   // wave.vs + wave.fs, loaded once
renderer.set_uniform(wave, "time", seconds);
renderer.begin_shader(wave);
renderer.draw_sprite(sprite);
renderer.end_shader();
```
- Texture names never travel as text: the scripts declare them (`textures { ... }`, see the [Lua API](reference/lua-api.md#global-functions)), the server lists them in the `texture` category of the string table and snapshots carry their 16-bit id. The client turns the id back into the name before asking its cache.

#### Preloading and the loading screen

The client does not show the game until every asset it may need is loaded, so the first explosion or the first enemy of a type never stalls a frame:

- Until `connect_accept` arrives, and then until preloading ends, each frame draws the **loading screen** instead of the game: a bar filled with the progress and a `loaded / total` count above it, drawn with `draw_rect` and `draw_text` only (no asset needed).
- Once connected, every name of the `texture`, `sound` and `music` categories of the string table is queued and loaded through the caches above, **4 assets per frame** on the main thread. The window stays responsive and the backends stay on their thread.
- Each frame still runs `NetworkClient::update()`, so heartbeats keep the session alive whatever the total loading time; only a single asset taking longer than the 5 s timeout could end it. Events (sounds, music, script events) are handled as usual. Snapshots are received but not drawn, the scripts' `update(dt)` does not run and no input is sent.
- A missing or rejected asset is logged once by its cache and counted; when the last asset is done the client logs `client: preloaded N assets, M missing` and the game starts, drawing placeholders for the missing ones. Sounds and musics are skipped while the audio device is closed.
- Loading on first use stays as the fallback for anything not preloaded.
- The first game frame comes right after the last asset is loaded, in the same frame.

---

## 10. Lua scripting API

The Lua API has its own page: [Lua API reference](reference/lua-api.md).

---

## 11. Engine rules and conventions

- Nothing in `engine/` knows about any game. Test for every addition: *would this make sense in a platformer?*
- Public engine symbols are marked with the export macro generated by CMake; default visibility is hidden on every platform.
- No global state in headers (no `static` variables in templates): registries and singletons live in a `.cpp` of `engine-core`, behind exported functions. Component identity uses stable names, not per-module counters.
- Third-party headers are never included in public engine headers.
- Resources are referenced by name (`"sprites/ship.png"`); the asset cache (§9.5) loads and caches them.
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
- **Packaging:** `cmake --install` produces a self-contained `bin/` folder with the editor, the runtime, the engine libraries and `plugins/`, plus `examples/`. Games are packaged by the editor's export. CPack archives on top of it are planned.

---

## 14. Documentation

All documentation is in English and published online.

| Document | Where |
|---|---|
| README: description, dependencies, build, quick start | [README](https://github.com/Saitamadupauvre/RType#readme) |
| Getting started | [getting-started.md](getting-started.md) |
| Making games: editor, scripting, playing, exporting | [guide/](guide/index.md) |
| Lua API reference | [reference/lua-api.md](reference/lua-api.md) |
| Example game | [examples/r-type.md](examples/r-type.md) |
| Architecture diagrams, modules, engine rules | [architecture.md](architecture.md), [engine.md](engine.md), [networking.md](networking.md), [plugins.md](plugins.md), [editor.md](editor.md), [editor-app.md](editor-app.md) |
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
| Starfield | Removed with the generic `ClientApp`; to be redone as a shader |
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
2. **Standalone engine:** `engine/` moves to its own repository; the editor and the runtime consume it as a dependency.
3. **Generic runtime:** done. `engine-runtime` runs any game folder in the dedicated, client, host and solo modes (§6); R-Type is a game folder in `examples/r-type/`, with no C++ of its own.
4. **Editor:** done so far: scenes, viewport, inspector, undo/redo, Play, Export, command palette, settings. Next: a launcher to create and open games, scene management in the UI, asset browser.
5. **Tooling:** hot reload of scripts, developer console executing Lua (done, see the [Lua API](reference/lua-api.md#developer-console-and-console-variables)).
6. **Second game:** a different game (e.g. a platformer or networked Breakout) written only in Lua and assets, proving the engine is generic.

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
