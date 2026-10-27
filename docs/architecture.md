# Architecture

This page gives the big picture: the layers, the subsystems, how data flows
during a tick and a frame, and where each piece lives in the code. Details of
each part are in [Engine modules](engine.md),
[Networking internals](networking.md), [Plugins](plugins.md) and
[Game](game.md).

## Principles

1. **The engine never knows the game.** Nothing under `engine/` names R-Type,
   its entities or its scripts. A feature goes into the engine only if it
   would make sense in another game (a platformer, a Breakout).
2. **The server is authoritative.** Clients send inputs, never positions. The
   server runs every game rule and replicates the result.
3. **Subsystems are decoupled.** Rendering, networking and game logic live in
   separate shared libraries and talk through the ECS and the event bus, never
   by calling each other.
4. **Backends are plugins.** The Raylib backend is loaded at runtime; the
   server never loads it and runs without a display.
5. **Time is fixed-step.** Game logic advances in fixed 1/60 s steps driven by
   the engine clock, never by the frame rate or the CPU speed.

## Layers

```mermaid
flowchart TB
    subgraph Game["Game layer (game/)"]
        LUA["Lua scripts<br/>game/scripts"]
        SRV["r-type_server<br/>ServerApp"]
        CLI["r-type_client<br/>SnapshotView, Starfield"]
    end
    subgraph Engine["Engine layer (engine/), shared libraries"]
        CORE["engine-core<br/>ECS, scheduler, event bus,<br/>clock, fixed-step loop, log, DynamicLibrary"]
        SCRIPT["engine-script<br/>Lua runtime, declarations,<br/>entity handles, players"]
        NET["engine-network<br/>UDP sockets, protocol, threads,<br/>sessions, snapshots, stats"]
        PHYS["engine-physics<br/>bodies, colliders, collisions"]
        GFX["engine-graphics<br/>IRenderer, IInput,<br/>plugin loader"]
        AUD["engine-audio<br/>IAudio, NullAudio,<br/>plugin loader"]
    end
    subgraph Plugins["Plugins (plugins/), loaded at runtime"]
        RAY["graphics-raylib<br/>window, 2D rendering, input"]
        RAU["audio-raylib<br/>sound effects, music"]
    end
    LUA -. loaded by .-> SCRIPT
    SRV --> SCRIPT
    SRV --> NET
    SRV --> CORE
    CLI --> NET
    CLI --> GFX
    CLI --> AUD
    CLI --> CORE
    SCRIPT --> CORE
    NET --> CORE
    PHYS --> CORE
    GFX --> CORE
    GFX -. dlopen .-> RAY
    AUD --> CORE
    AUD -. dlopen .-> RAU
```

| Layer | Knows about | Never knows about |
|---|---|---|
| Game | Public engine headers (`engine/include/`) | Engine internals, Raylib, sockets, Lua C API |
| Engine | Its own modules, third-party libraries privately | The game, the plugins' backend libraries |
| Plugins | Engine interfaces and types | The game |

Third-party headers stay private: `raylib.h` appears only in
`plugins/graphics-raylib/`, POSIX socket headers only in
`engine/src/network/socket/`, Lua and sol2 only in `engine/src/script/`.

## Subsystems

| Subsystem | Module | Main types | Role |
|---|---|---|---|
| ECS | `engine-core` | `EntityManager`, `ComponentPool`, `Entity` | Entities are generational ids; components live in sparse-set pools |
| Systems | `engine-core` | `SystemScheduler` | Runs systems in a fixed order; a throwing system is disabled and logged |
| Events | `engine-core` | `EventBus`, `Subscription` | Typed publish/subscribe between subsystems |
| Time | `engine-core` | `IClock`, `SteadyClock`, `ManualClock`, `FixedStepLoop` | Fixed-step simulation, injectable clock for tests |
| Logging | `engine-core` | `log::info/warn/error`, `Sink`, `MemorySink` | Levelled logs on stderr; tests capture them |
| Scripting | `engine-script` | `ScriptRuntime`, `ScriptRuntimeBuilder`, `ServerApi`, `ClientApi` | Loads and sandboxes Lua, spawns scripted entities, runs callbacks; API sets give each side its own Lua functions |
| Networking | `engine-network` | `NetworkServer`, `NetworkClient` | UDP transport, protocol, sessions, replication |
| Physics | `engine-physics` | `PhysicsSystem`, `PhysicsBody`, `AABBCollider` | Integration and collision detection/resolution; the server uses it to find hitbox overlaps |
| Rendering and input | `engine-graphics` + plugin | `IRenderer`, `IInput`, `load_graphics_plugin` | Backend-independent drawing and keyboard/mouse input |

## Class diagram

The main public engine types and the plugin backends that implement them.
Method lists are abridged; the headers in `engine/include/engine/` are the
reference.

```mermaid
classDiagram
    direction TB

    namespace core {
        class Entity {
            +index() uint32
            +generation() uint32
            +id() uint64
        }
        class EntityPool {
            +alive(Entity) bool
            +size() size_t
        }
        class ComponentPool~T~ {
            +emplace(Entity, Args...) T&
            +erase(Entity) bool
            +get(Entity) T&
            +find(Entity) T*
            +entities() span~Entity~
        }
        class EntityManager {
            +spawn() Entity
            +kill(Entity) bool
            +alive(Entity) bool
            +register_component~T~()
            +add~T~(Entity, T) T&
            +get~T~(Entity) T&
            +has~T~(Entity) bool
            +query~T~() ComponentQuery
        }
        class SystemScheduler {
            +add(name, order, SystemFunction) SystemHandle
            +remove(SystemHandle) bool
            +run(EntityManager&, float dt)
        }
        class EventBus {
            +subscribe~E~(handler) Subscription
            +unsubscribe(Subscription)
            +publish~E~(const E&)
            +enqueue~E~(E)
            +dispatch()
        }
        class IClock {
            <<interface>>
            +now() Duration
        }
        class SteadyClock
        class ManualClock {
            +advance(Duration)
        }
        class FixedStepLoop {
            +run_frame(Update) size_t
            +alpha() double
            +step() Duration
        }
        class DynamicLibrary {
            +symbol~F~(name) F
            +error() string
            +close()
        }
    }

    namespace script {
        class ScriptRuntime {
            +builder()$ ScriptRuntimeBuilder
            +api~Set~() Set&
            +load_scripts(path) bool
            +attach_world(EntityManager&)
            +spawn(type, ...) optional~Entity~
            +update(float dt)
            +collide(Entity, Entity)
            +flush_destroyed() size_t
        }
        class ScriptRuntimeBuilder {
            +with~Set~() ScriptRuntimeBuilder&
            +build() ScriptRuntime
        }
        class ApiSet {
            <<abstract>>
            +section() string_view
        }
        class ServerApi {
            +player_joined(id) bool
            +player_left(id) bool
            +set_player_input(id, InputState)
            +take_audio_commands() vector~AudioCommand~
            +take_event_commands() vector~EventCommand~
        }
        class ClientApi {
            +dispatch(EventCommand) bool
        }
        ScriptRuntimeBuilder ..> ScriptRuntime : builds
        ScriptRuntime *-- ApiSet : owns
        ApiSet <|-- ServerApi
        ApiSet <|-- ClientApi
    }

    namespace network {
        class NetworkServer {
            +start(error_code&) bool
            +stop()
            +receive()
            +inputs() vector~PlayerInput~
            +send_snapshot(EntityManager&)
            +stats() vector~PlayerStats~
        }
        class EntitySnapshot {
            +entity_id uint32
            +type_id uint16
            +fields vector~ReplicatedValue~
            +find(component, field) FieldValue
        }
        class NetworkClient {
            +connect(error_code&) bool
            +update()
            +send_input(InputState)
            +disconnect()
            +state() ConnectionState
            +strings() StringTable
            +entities() vector~EntitySnapshot~
            +stats() NetworkStats
        }
        class Replicated {
            +type_id uint16
            +owner uint8
        }
    }

    namespace physics {
        class PhysicsSystem {
            +update(float dt)
            +set_gravity(Vec2)
            +get_gravity() Vec2
        }
    }

    namespace graphics {
        class IGraphicsBackend {
            <<interface>>
            +renderer() IRenderer&
            +input() IInput&
        }
        class IRenderer {
            <<interface>>
            +open(WindowConfig) bool
            +load_texture(path) TextureId
            +begin_frame(Color)
            +draw_sprite(SpriteDraw)
            +draw_text(text, position, size, color)
            +end_frame()
        }
        class IInput {
            <<interface>>
            +is_key_down(Key) bool
            +is_key_pressed(Key) bool
            +mouse_position() Vec2
        }
        class GraphicsPlugin {
            +backend() IGraphicsBackend&
        }
    }

    namespace audio {
        class IAudio {
            <<interface>>
            +open() bool
            +update()
            +load_sound(path) SoundId
            +play_sound(SoundId, volume, pitch)
            +load_music(path) MusicId
            +play_music(MusicId, loop)
        }
        class NullAudio
        class AudioPlugin {
            +backend() IAudio&
        }
    }

    namespace plugins {
        class RaylibBackend
        class RaylibRenderer
        class RaylibInput
        class RaylibAudio
    }

    EntityManager *-- EntityPool
    EntityManager *-- "many" ComponentPool~T~
    EntityPool ..> Entity : hands out
    SystemScheduler ..> EntityManager : runs systems on
    IClock <|.. SteadyClock
    IClock <|.. ManualClock
    FixedStepLoop --> IClock

    ScriptRuntime --> EntityManager : attached world
    NetworkServer --> IClock
    NetworkServer --> EventBus : PlayerJoined / PlayerLeft
    NetworkServer ..> Replicated : reads to build snapshots
    NetworkServer ..> FieldRegistry : reads replicated fields
    NetworkClient ..> FieldRegistry : resolves field names
    NetworkClient --> IClock
    NetworkClient --> EventBus
    PhysicsSystem --> EntityManager
    PhysicsSystem --> EventBus : CollisionEvent

    IGraphicsBackend --> IRenderer
    IGraphicsBackend --> IInput
    GraphicsPlugin *-- DynamicLibrary
    GraphicsPlugin *-- IGraphicsBackend
    AudioPlugin *-- DynamicLibrary
    AudioPlugin *-- IAudio
    IAudio <|.. NullAudio

    IGraphicsBackend <|.. RaylibBackend
    IRenderer <|.. RaylibRenderer
    IInput <|.. RaylibInput
    RaylibBackend *-- RaylibRenderer
    RaylibBackend *-- RaylibInput
    IAudio <|.. RaylibAudio
```

## Executables

```mermaid
flowchart LR
    subgraph Server["r-type_server process"]
        direction TB
        RX["receive thread"] -- inbox --> GT["game thread<br/>ServerApp::tick"]
        GT -- outbox --> TX["send thread"]
    end
    subgraph Client["r-type_client process"]
        direction TB
        CRX["receive thread"] -- inbox --> MT["main thread<br/>render loop"]
        MT -- outbox --> CTX["send thread"]
    end
    TX -- "UDP: snapshots, events" --> CRX
    CTX -- "UDP: inputs, heartbeats" --> RX
```

Both processes use the same `NetworkIo` core: one thread reads the socket, one
thread writes to it, and the game or render thread only touches two queues.
See [Networking internals](networking.md).

### One server tick (60 Hz)

`ServerApp::tick` in `game/server/ServerApp.cpp`:

```mermaid
sequenceDiagram
    participant Net as NetworkServer
    participant App as ServerApp
    participant Lua as ScriptRuntime + ServerApi
    participant ECS as EntityManager
    App->>Net: receive()
    Note over Net: sessions, timeouts,<br/>PlayerJoined / PlayerLeft on the event bus
    Net-->>Lua: on_player_joined / on_player_left
    App->>Net: inputs()
    App->>Lua: set_player_input(player, input)
    App->>Lua: update(dt)
    Note over Lua: every entity's update(self, dt), spawns
    App->>ECS: Collisions::step(dt): PhysicsSystem
    Note over ECS: CollisionEvent on the event bus
    ECS-->>Lua: collide(a, b) → on_collide(self, other)
    App->>Lua: flush_destroyed()
    Note over Lua: on_destroy, entities removed
    App->>ECS: systems_.run(dt)
    App->>ECS: replicate(): Replicated{type, owner}
    App->>Net: send_snapshot(registry)
```

### One client frame

`game/client/main.cpp`:

1. `network.update()`: apply received snapshots and events, retry the
   connection, send heartbeats, detect a silent server.
2. Capture every key and mouse button held, and the mouse position, through
   `IInput` (`engine::graphics::capture_input`).
3. In fixed 1/60 s steps: scroll the starfield and send the input to the
   server.
4. Draw the starfield, then every replicated entity with
   `assets/sprites/<type>.png` tinted with its colour, then the marker above
   the local player's ship. `SnapshotView` mirrors each entity into a local
   world by setting its replicated fields through the field registry, skipping
   components that world does not register (such as `PhysicsBody`).

The client never runs game rules: it draws what the server sent.

## Data ownership

| Data | Owner | Who reads it |
|---|---|---|
| World state (entities, components) | Server game thread | Snapshot builder, on the same thread |
| Lua state | Server game thread | Nobody else: Lua is single-threaded |
| Sessions, sequences, statistics | Game thread (server) / main thread (client) | Same thread only, no lock |
| Inbox / outbox | `MessageQueue`, mutex-protected | Receive thread pushes, game thread drains; game thread pushes, send thread drains |
| Socket | `NetworkIo` | Receive thread reads, send thread writes |
| Window, textures, input | Client main thread | Graphics calls happen only there |

## Code map

| Concern | Where |
|---|---|
| Public engine API | `engine/include/engine/<module>/` |
| Engine implementation | `engine/src/<module>/` |
| CMake helpers (`engine_add_module`, `engine_add_plugin`) | `engine/cmake/EngineModule.cmake` |
| Raylib backends | `plugins/graphics-raylib/src/`, `plugins/audio-raylib/src/` |
| Server wiring | `game/server/` (`main.cpp`, `ServerApp`, `Cli`) |
| Client wiring | `game/client/` (`main.cpp`, `SnapshotView`, `Starfield`, `AudioPlayer`, `Cli`) |
| Field registry shared by both | `game/common/GameFields` |
| Game rules | `game/scripts/` |
| Tests | `tests/engine/<module>/`, `tests/game/<target>/`, `tests/plugins/<name>/` |

## Where this is going (Part 2)

The `main` functions become generic `ServerApp` / `ClientApp` classes in the
engine, the engine moves to its own repository consumed through vcpkg, and a
second game written only in Lua proves it is generic. See the
[roadmap](PROJECT.md#15-roadmap).
