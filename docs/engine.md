# Engine modules

The engine lives in `engine/` and is split into modules. Each module is a
shared library (`libengine-<name>.so` / `engine-<name>.dll`) created with the
`engine_add_module` CMake helper from `engine/cmake/EngineModule.cmake`.
The [architecture](architecture.md) page shows how they fit together.

## Modules

| Module | Target | Depends on | Third-party (private) |
|---|---|---|---|
| core | `engine::core` | — | `dl` |
| script | `engine::script` | core (physics, navigation privately) | Lua 5.4, sol2, nlohmann-json |
| network | `engine::network` | core | POSIX sockets |
| physics | `engine::physics` | core | — |
| navigation | `engine::navigation` | — | — |
| graphics | `engine::graphics` | core | — (backends are [plugins](plugins.md)) |
| audio | `engine::audio` | core | — (backends are [plugins](plugins.md)) |
| app | `engine::app` | core, script, network, graphics, audio (physics privately) | — |
| editor | `engine::editor` | core, script | — |

### core

The foundation every other module builds on.

| Area | Headers | What it gives |
|---|---|---|
| Entities | `Entity.hpp`, `EntityPool.hpp` | 32-bit ids with a generation counter: a recycled id never matches a destroyed entity |
| Components | `ComponentPool.hpp`, `ComponentTypeId.hpp`, `EntityManager.hpp`, `ComponentQuery.hpp` | Sparse-set pools, `spawn`/`kill`/`add`/`get`/`has`, `query<A, B>().each(...)` |
| Common components | `Components.hpp`, `ComponentPhysics.hpp`, `Transform.hpp` | `Position`, `Sprite` (path + colour), `Tags`, optional `Rotation` and `Scale` read with `rotation_of` / `scale_of`, which return 0 and (1, 1) when the entity has none |
| Systems | `SystemScheduler.hpp` | Named systems run by ascending order; one that throws is disabled and logged, the others keep running |
| Events | `event/EventBus.hpp` | Typed publish/subscribe; a `Subscription` unsubscribes when destroyed |
| Time | `time/IClock.hpp`, `SteadyClock.hpp`, `ManualClock.hpp`, `FixedStepLoop.hpp` | Fixed-step loop (60 Hz by default) over an injectable clock, so tests control time |
| Logging | `log/Log.hpp`, `log/Sink.hpp`, `log/MemorySink.hpp` | `log::debug/info/warn/error` with `std::format` syntax; `MemorySink` captures logs in tests |
| Metrics | `metrics/Metrics.hpp`, `metrics/ProcessUsage.hpp` | Timers, counters and gauges with one sample per frame in a fixed-size history; `ScopedTimer`; process CPU and memory. See [Metrics](#metrics) |
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
- turns the world into data and back: `capture_instances()` returns one
  `Instance{name, type, position, overrides}` per scripted entity, and
  `spawn_instances(instances)` rebuilds them (see below).

### Instances

Scenes, the editor's Play/Stop and undo/redo store the world as **instances**
rather than raw component memory. An instance holds the entity's type, its
position, an optional name (`set_entity_name()`, never seen by scripts) and
its **overrides**: the editable registry fields and the script fields whose
value differs from the type's default, keyed `"Component.field"` or
`"Script.name"` and sorted.

```cpp
const std::vector<engine::script::Instance> saved = runtime.capture_instances();
const std::vector<engine::script::InstanceIssue> issues = runtime.spawn_instances(saved);
```

An issue gives the index of its instance, the skipped field (empty when the
whole instance was skipped) and a message.

- **Default** means the value a field gets from the prefab's native fields
  (`sprite`, `hitbox`, `vx`...) before `on_spawn` runs. What `on_spawn` sets
  is therefore stored as an override, and a component the prefab does not
  give has all its editable fields stored. Changing a prefab later updates
  every instance that did not override that field.
- **Spawning** an instance spawns its type at its position, names it, runs
  `on_spawn`, then writes the overrides through the field registry, so they
  win. Entities already in the world are kept: clearing it is the caller's
  job.
- **Failures** never throw: an unknown type or a system skips the instance,
  an unknown or non-editable field, a wrong kind or an out-of-range value
  skips that override. Each is returned as an `InstanceIssue` and logged.
- **Script fields** are the Lua free fields of a declaration (`health = 3`)
  whose default has a supported kind. `script_fields(type)` lists them with
  their kind and default, inferred once at load (rules in
  [Game folder and scenes](game-folder.md#script-fields)). Spawning writes a
  `"Script.name"` override into the entity's Lua table after `on_spawn`, so
  `self.health` reads it. A value of the wrong kind is skipped at spawn and
  left out (with a warning) at capture.
- Not captured: free fields that are not script fields (functions, `_private`
  names, unsupported tables, fields of a `server`/`client` section), non-editable fields such as
  `Replicated.owner`, entities marked with `self:destroy()` and entities the
  runtime did not spawn. An entity spawned by another one's `on_spawn` is
  captured too, so spawning the instances back spawns it twice.
- Scene files store instances as JSON: `read_scene()` reads one against the
  loaded declarations and `write_scene()` saves one, and `load_game()` spawns
  the `start_scene` of `game.json` before `on_start()`. Lua `load_scene(name)`
  requests a switch that `flush_scene()` applies at the end of the server tick,
  keeping entities declared `persistent = true`. The format, the switch, the
  reported problems and the API are in
  [Game folder and scenes](game-folder.md). The JSON code (`scene/`, built on
  `nlohmann-json`) stays private to the module.

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

### navigation

Grid pathfinding, with no dependency on any other module. A `NavGrid` lays
`width × height` square cells of `cell_size` world units over the world from an
`origin`; each cell is blocked or walkable with a movement cost of at least 1.
`block_area(min, max)` blocks every cell a world rectangle strictly overlaps,
`cell_at` / `center_of` convert between world points and cells. A grid holds at
most `NavGrid::max_cells` (2^20) cells.

`find_path(grid, start, goal, options)` runs A*: entering a cell costs its
cost, times √2 for a diagonal. `PathOptions` chooses:

| Option | Default | Meaning |
|---|---|---|
| `heuristic` | `Octile` | `Dijkstra` (no estimate), `Manhattan`, `Octile`, `Euclidean` |
| `diagonals` | `true` | 8-way moves; a diagonal needs both cells beside it walkable, so paths never cut wall corners |
| `max_expanded` | 10 000 | Search budget, so one search never stalls a tick |
| `smooth` | `false` | Run `smooth_path` on the result |

The result has a `status` (`Found`, `Unreachable`, `LimitReached`,
`OutsideGrid`, `Blocked`), the cells from start to goal (empty unless found),
the grid path cost and the number of expanded cells. Dijkstra, Octile and
Euclidean always give the cheapest path; Manhattan does too without diagonals,
and with diagonals it is faster but may return a slightly longer path. Searches
are deterministic.

`smooth_path` keeps the first and last cells and jumps from each kept cell to
the farthest following cell in straight `line_of_sight`, never through a
blocked cell, between two blocked corners, or through a cell costlier than the
part of the path it skips.

Scripts reach it through `engine-script`, which links it privately: `navgrid`
builds a `NavGrid` from the hitboxes of entities carrying obstacle tags (rebuilt
at most once per tick), `find_path` returns waypoints and `self:navigate_to`
steers an entity along them through its physics velocity. See
[the Lua API](PROJECT.md#106-global-functions).

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
runs any game folder (`game.json`, `scripts/`, `scenes/` and `assets/`, see
[Game folder and scenes](game-folder.md)) without knowing the game.

| Header | What it gives |
|---|---|
| `ServerApp.hpp` | Authoritative server over any `ServerTransport`: loads the game folder (scripts, then the start scene) with the server API, string table, 60 Hz tick (scripts, physics, collisions, systems, replication, audio and events) |
| `ClientApp.hpp` | Client over any `ClientTransport`, with injected `IRenderer`, `IInput` and `IAudio`: Lua with the client API, asset preloading behind a loading screen, snapshot drawing, audio, local input to Lua and to the server |
| `Launch.hpp` | `Mode` (`dedicated`, `client`, `host`, `solo`), `LaunchOptions` and `launch()`: picks the transports, loads the plugins and runs the apps; `host` runs the server on a background thread, `solo` runs both in one thread over a `LoopbackNetwork` |

Its private helpers (`SnapshotView`, `WorldDraw`, `AudioPlayer`,
`Collisions`) are tested in `tests/engine/app/`, with a fixture game in
`tests/fixtures/app/game/` played over a `LoopbackNetwork`.

### editor

The editing model behind the editor front: `EditorSession` opens a game folder
with the `editor` API set, holds the open scene as a `SceneDocument`, applies
edits with undo and redo, and runs the scene commands; every command returns a
`CommandResult`. Its edit world is built with
`ScriptRuntime::preview_instances()`, which builds prototypes only, so no Lua
callback runs. See [Editor backend](editor.md). Tested in
`tests/engine/editor/` with the fixture game `tests/fixtures/editor/game/`.

## Metrics

`engine::core::metrics::Metrics` measures where time goes. `ServerApp` and
`ClientApp` each own one, returned by `metrics()`, for the metrics overlay and
the developer console to read.

```cpp
using namespace engine::core::metrics;

Metrics metrics(clock);
{
    const ScopedTimer timer(metrics, "lua.update");
    runtime.update(dt);
}
metrics.count("network.snapshots");
metrics.set_gauge("entities", 42);
metrics.end_frame();

const auto lua = metrics.summary("lua.update");
const std::vector<double> frame_times = metrics.history("frame");
```

| Kind | Written with | Sample of a frame |
|---|---|---|
| timer | `record_time(name, duration)`, `ScopedTimer` | Sum of the durations of the frame, in milliseconds; 0 if none |
| counter | `count(name, amount = 1)` | Sum of the amounts of the frame; 0 if none |
| gauge | `set_gauge(name, value)` | Last value set, kept until set again |

- `end_frame()` adds one sample to every known metric. The latest `capacity`
  samples (120 by default, 2 s at 60 Hz) are kept in a ring buffer.
- `summary(name)` gives `last`, `average`, `max` and the sample count, and
  `history(name)` gives the samples from oldest to newest (for a graph).
  `names()` lists every metric.
- A metric keeps the kind of its first write. A write of another kind, an
  empty name or a non-finite value is dropped and logged once per name.
- Thread safe. In host mode the client thread can read the server's metrics
  while the server thread writes them.
- `set_enabled(false)` makes every write and `end_frame()` return at once,
  without locking. A `ScopedTimer` then does not read the clock. The history
  already recorded is kept.
- The clock is injected, so with a `ManualClock` tests control every duration.
- `ProcessMonitor` samples the process at most every 500 ms (with
  `sample_process()` or an injected sampler). It publishes `process.cpu`, in
  percent of one core, and `process.memory`, in MiB.

### ServerApp metrics

One frame per tick.

| Name | Kind | What |
|---|---|---|
| `tick` | timer | Whole tick |
| `network.receive` | timer | Packets received and inputs handed to the scripts |
| `lua.update` | timer | `update(dt)` of the scripts |
| `physics` | timer | Movement, collisions and their script callbacks, destroyed entities |
| `systems` | timer | Native systems and camera follow |
| `network.send` | timer | Replication, audio, events and snapshot |
| `entities` | gauge | Living entities |
| `players` | gauge | Connected players |
| `snapshot.bytes` | gauge | Bytes of the latest snapshot, sent once to every player |
| `process.cpu`, `process.memory` | gauge | CPU (% of one core) and resident memory (MiB) |

### ClientApp metrics

One frame per call to `frame()`.

| Name | Kind | What |
|---|---|---|
| `frame` | timer | Time since the previous frame (frame time) |
| `fps` | gauge | `1 / frame` |
| `network.update` | timer | Network read and write of the frame |
| `lua.update` | timer | `update(dt)` of the scripts, every tick of the frame |
| `physics` | timer | Movement of the local entities and destroyed entities, every tick of the frame |
| `draw` | timer | Loading screen or world drawing |
| `entities` | gauge | Entities of the latest snapshot |
| `network.snapshots` | counter | 1 on a frame showing a newer snapshot |
| `network.rtt` | gauge | Round-trip time in ms, once the server answered a heartbeat |
| `network.packets_lost` | gauge | Packets lost since connecting |
| `network.packets_in`, `network.packets_out` | gauge | Packets per second |
| `network.bytes_in`, `network.bytes_out` | gauge | Bytes per second |
| `process.cpu`, `process.memory` | gauge | CPU (% of one core) and resident memory (MiB) |

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
