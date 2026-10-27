# Engine modules

The engine lives in `engine/` and is split into modules. Each module is a
shared library (`libengine-<name>.so` / `engine-<name>.dll`) created with the
`engine_add_module` CMake helper from `engine/cmake/EngineModule.cmake`.
The [architecture](architecture.md) page shows how they fit together.

## Modules

| Module | Target | Depends on | Third-party (private) |
|---|---|---|---|
| core | `engine::core` | — | `dl` |
| script | `engine::script` | core | Lua 5.4, sol2 |
| network | `engine::network` | core | POSIX sockets |
| physics | `engine::physics` | core | — |
| graphics | `engine::graphics` | core | — (backends are [plugins](plugins.md)) |
| audio | `engine::audio` | core | — (backends are [plugins](plugins.md)) |
| app | `engine::app` | core, script, network, graphics, audio (physics privately) | — |

### core

The foundation every other module builds on.

| Area | Headers | What it gives |
|---|---|---|
| Entities | `Entity.hpp`, `EntityPool.hpp` | 32-bit ids with a generation counter: a recycled id never matches a destroyed entity |
| Components | `ComponentPool.hpp`, `ComponentTypeId.hpp`, `EntityManager.hpp`, `ComponentQuery.hpp` | Sparse-set pools, `spawn`/`kill`/`add`/`get`/`has`, `query<A, B>().each(...)` |
| Common components | `Components.hpp`, `ComponentPhysics.hpp` | `Position`, `Sprite` (path + colour), `Tags` |
| Systems | `SystemScheduler.hpp` | Named systems run by ascending order; one that throws is disabled and logged, the others keep running |
| Events | `event/EventBus.hpp` | Typed publish/subscribe; a `Subscription` unsubscribes when destroyed |
| Time | `time/IClock.hpp`, `SteadyClock.hpp`, `ManualClock.hpp`, `FixedStepLoop.hpp` | Fixed-step loop (60 Hz by default) over an injectable clock, so tests control time |
| Logging | `log/Log.hpp`, `log/Sink.hpp`, `log/MemorySink.hpp` | `log::debug/info/warn/error` with `std::format` syntax; `MemorySink` captures logs in tests |
| Plugins | `DynamicLibrary.hpp` | RAII wrapper over `dlopen` / `LoadLibrary` |

Component types are identified by stable names registered in a `.cpp` of
`engine-core`, so ids are identical in every shared library.

### script

`ScriptRuntime` embeds a sandboxed Lua 5.4 state (through sol2):

- loads every `.lua` file of a folder recursively, registers `entity` and
  `system` declarations, rejects duplicates and reserved names;
- opens only `base`, `math`, `string`, `table`, `coroutine`; removes `dofile`,
  `loadfile` and bytecode loading; `require` only reaches `lib/`;
- spawns scripted entities, maps native fields (`x`, `y`, `vx`, `vy`,
  `sprite`, `color`, `tags`, `hitbox`, `collides_with`) to C++ components and
  keeps free fields in Lua; `vx` / `vy` are the velocity of the entity's
  `PhysicsBody` and `hitbox` its `AABBCollider` (`engine-physics`); setting
  one of them gives the entity a dynamic, non-resolving body without gravity;
- calls `update`, `on_spawn`, `on_destroy` and the `main.lua` callbacks
  through protected calls: an error is logged with file and line and the game
  continues;
- registers the sounds, musics and events declared with `sounds { ... }`,
  `musics { ... }` and `event "name" { ... }` (`sound_names()`,
  `music_names()`, `event_names()`);
- turns overlaps reported by the host (`collide(first, second)`) into
  `on_collide(self, other)` calls, filtered by each entity's `collides_with`
  tags.

The runtime only holds this core API. Everything that belongs to one side is an
**API set**, chosen once with a builder and owned by the runtime:

```cpp
auto runtime = engine::script::ScriptRuntime::builder()
                   .with<engine::script::ServerApi>()
                   .build();
auto& server = runtime.api<engine::script::ServerApi>();
```

- `ServerApi`: `spawn`, `players`, `find_player`, `play`, `music`, `emit`,
  `self.input`, `self.owner`; tracks players and routes each player's input
  to the entities they own (`player_joined()`, `set_player_input()`), and
  queues audio and event requests for the host (`take_audio_commands()`,
  `take_event_commands()`).
- `ClientApi`: `on(name, handler)`; `dispatch(event)` calls the handlers of
  an event received from the server.
- `EditorApi`: empty for now.

A function of a set that was not added does not exist in the sandbox, so
calling it is a plain Lua error. Each set names the declaration section it
reads (`server = { ... }`, `client = { ... }`). Adding a set twice, or both
`ServerApi` and `ClientApi`, makes `build()` throw. Sources follow the same
split: public-API implementations at the root of `engine/src/script/`,
internals in `lua/`, `loading/`, `entity/`, `audio/`, `event/` and `player/`.

The Lua API is specified in [PROJECT.md §10](PROJECT.md#10-lua-scripting-api).

### network

UDP transport, binary protocol, network threads, sessions, snapshots and
statistics behind two classes, `NetworkServer` and `NetworkClient`. See
[Networking internals](networking.md) and the [protocol](protocol.md).

### physics

`PhysicsSystem` integrates forces, acceleration and velocity of entities with
a `PhysicsBody` (static or dynamic, mass, gravity, collision layer and mask),
detects overlaps between `AABBCollider` and `CircleCollider` shapes, resolves
them and publishes collision events on the event bus once every pair has
been checked, so handlers may spawn or destroy entities. The R-Type server
runs it every tick and forwards each `CollisionEvent` to the script runtime;
scripted entities only use it for detection (`resolve_collisions = false`).

### graphics

Backend-independent interfaces (`IRenderer`, `IInput`, `IGraphicsBackend`),
engine types (`Color`, `Rect`, `SpriteDraw`, `Key`...) and
`load_graphics_plugin()`, which loads a backend at runtime. See
[Plugins](plugins.md).

### audio

Backend-independent `IAudio` interface (sound effects, streamed music, master,
music and effects volumes), `SoundId` / `MusicId`, `NullAudio` (a silent
backend) and `load_audio_plugin()`, which loads a backend at runtime. See
[Plugins](plugins.md#audio-raylib).

### app

The composition root: the only module that depends on most of the others. It
runs any game folder (`scripts/` and `assets/`) without knowing the game.

| Header | What it gives |
|---|---|
| `ServerApp.hpp` | Authoritative server over any `ServerTransport`: Lua with the server API, string table, 60 Hz tick (scripts, physics, collisions, systems, replication, audio and events) |
| `ClientApp.hpp` | Client over any `ClientTransport`, with injected `IRenderer`, `IInput` and `IAudio`: Lua with the client API, snapshot drawing, audio, local input to Lua and to the server |
| `Launch.hpp` | `Mode` (`dedicated`, `client`, `host`, `solo`), `LaunchOptions` and `launch()`: picks the transports, loads the plugins and runs the apps; `host` runs the server on a background thread, `solo` runs both in one thread over a `LoopbackNetwork` |

Its private helpers (`SnapshotView`, `WorldDraw`, `AudioPlayer`,
`Collisions`) are tested in `tests/engine/app/`, with a fixture game in
`tests/fixtures/app/game/` played over a `LoopbackNetwork`.

## The contract

A module promises three things to the code that uses it:

1. **A stable target name**: `engine::<name>`. Nobody links `engine-<name>`
   directly. When the engine moves to its own repository, the same name comes
   from `find_package(engine)` and the game does not change.
2. **A public API made only of engine types**: headers in
   `engine/include/engine/<name>/`, with every public symbol marked
   `ENGINE_<NAME>_EXPORT`. No third-party header (Lua, Raylib, POSIX sockets...) ever
   appears there.
3. **Nothing else**: every symbol not marked with the export macro is hidden,
   and everything in `engine/src/<name>/` is private.

In return, a module may only depend on:

- other engine modules (`DEPENDS`, linked PUBLIC);
- third-party libraries from vcpkg (`PRIVATE_DEPENDS`, linked PRIVATE).

It never depends on the game or on a plugin.

## Declaring a module

```cmake
engine_add_module(script
    SOURCES src/script/Module.cpp
    DEPENDS engine::core
    PRIVATE_DEPENDS sol2::sol2
)
```

| Argument | Meaning |
|---|---|
| `script` | Module name: target `engine-script`, alias `engine::script`. |
| `SOURCES` | `.cpp` files, relative to `engine/`. |
| `DEPENDS` | Engine modules. PUBLIC: users of `engine::script` also get them. |
| `PRIVATE_DEPENDS` | Third-party libraries. PRIVATE: their headers stay inside the module. |

What the helper does:

| Step | Result |
|---|---|
| `add_library(engine-script SHARED ...)` | Shared library in `bin/`. |
| `add_library(engine::script ALIAS ...)` | Stable name for users. |
| `generate_export_header` | `<engine/script/Export.hpp>` defining `ENGINE_SCRIPT_EXPORT`. |
| `CXX_VISIBILITY_PRESET hidden` | Symbols hidden by default, on Linux as on Windows. |
| Include directories | `engine/include/` and the generated folder are public. |
| `engine_set_warnings` | Engine warning flags, errors when `ENGINE_WARNINGS_AS_ERRORS` is on. |
| `install(TARGETS ...)` | Installed into `bin/`, next to the executables. |

## The export macro

`ENGINE_SCRIPT_EXPORT` expands differently depending on who compiles:

| Compiled as part of | Linux / macOS | Windows |
|---|---|---|
| `engine-script` itself | `__attribute__((visibility("default")))` | `__declspec(dllexport)` |
| Anything using it | `__attribute__((visibility("default")))` | `__declspec(dllimport)` |

```cpp
#include "engine/script/Export.hpp"

namespace engine::script {

ENGINE_SCRIPT_EXPORT void load_game(std::string_view folder);

class ENGINE_SCRIPT_EXPORT ScriptRuntime {
public:
    void update(float dt);
};

void internal_helper();

} // namespace engine::script
```

`load_game` and `ScriptRuntime` are visible outside the library; `internal_helper`
is not. Because symbols are hidden on every platform, forgetting the macro
fails on Linux as well.

## Rules

- Public headers include only the standard library, engine headers and
  `Export.hpp`.
- No global state in headers: registries live in a `.cpp` of `engine-core`
  behind exported functions.
- `engine/` uses only its own CMake helpers and variables, never root ones, so
  it stays extractable.
- A module talks to another through the ECS and the event bus, not by calling
  it directly.

## Adding a module

1. Create `engine/include/engine/<name>/` and `engine/src/<name>/`.
2. Add `engine_add_module(<name> SOURCES ... DEPENDS engine::core)` to
   `engine/CMakeLists.txt`.
3. Add tests in `tests/engine/<name>/` and register them with
   `engine_add_test(<name> SOURCES ...)` in `tests/CMakeLists.txt`.
4. Add any new third-party library to `vcpkg.json` and link it with
   `PRIVATE_DEPENDS`.
5. Link `engine::<name>` from `game/CMakeLists.txt` where needed.
