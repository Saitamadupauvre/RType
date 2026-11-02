# Technical and comparative study

Why we use each technology, algorithm and data structure, compared with the
alternatives we considered. Each section ends with the decision and the
trade-off we accept.

## Summary

| Area | Choice | Main alternatives |
|---|---|---|
| Language | C++20 | C++17, Rust |
| Build system | CMake + presets | Meson, xmake, Premake |
| Package manager | vcpkg (manifest mode) | Conan, CMake CPM |
| Rendering, window, input | Raylib, isolated in a runtime plugin | SFML, SDL |
| Networking | POSIX sockets wrapped in `engine-network` | Asio, ENet, SFML Network |
| Transport | UDP only | TCP, UDP + TCP |
| Scripting | Lua 5.4 + sol2 | LuaJIT, Python, no scripting |
| Engine architecture | ECS with sparse sets | Inheritance tree, archetype ECS |
| Tests | GoogleTest | Catch2, doctest |
| Command line | clap (our own, served by vcpkg) | CLI11, cxxopts |
| Logging | Small logger in `engine-core` | spdlog |
| Documentation | Markdown + MkDocs Material, Doxygen for the API | Sphinx, Docusaurus, GitBook, wiki |
| CI | GitHub Actions | GitLab CI, Jenkins |

## Language: C++20

The subject imposes C++. Among its versions:

| | C++17 | C++20 | C++23 |
|---|---|---|---|
| Concepts, `std::span`, designated initializers, `std::format`, `std::jthread` | No | Yes | Yes |
| Support in GCC, Clang, Apple Clang, MSVC | Full | Full for what we use | Partial (Apple Clang lags) |

**Decision:** C++20. `std::jthread` and `std::stop_token` simplify the network
threads, `std::span` makes the byte readers safe, designated initializers make
configuration structs readable, concepts constrain templates. C++23 would
force us to drop or work around Apple Clang.

## Build system: CMake

| | CMake | Meson | xmake | Premake |
|---|---|---|---|---|
| vcpkg / Conan integration | Native toolchain file | Through wrappers | Own package manager | Manual |
| IDE support (CLion, VS Code, Visual Studio) | Native | Good | Limited | Generates projects |
| Team experience | All members | None | None | None |
| Presets for CI and local builds | `CMakePresets.json` | No equivalent | Partial | No |

**Decision:** CMake with presets. Every build, test and check is a preset
(`ninja-debug`, `asan`, `tsan`, `tidy`, `ci`), so CI runs exactly what a
developer runs. The engine has its own helpers (`engine_add_module`,
`engine_add_plugin`) so it can be extracted into its own repository.

## Package manager: vcpkg

| | vcpkg | Conan | CMake CPM |
|---|---|---|---|
| Our dependencies available (Raylib, Lua, sol2, GoogleTest) | All | All | Fetches sources, builds them in the tree |
| Integration | One toolchain file | Profiles, generators, Python tool | CMake module |
| Reproducibility | `builtin-baseline` pins every version | Lockfiles | Git tags |
| Binary cache for CI | Built in (`VCPKG_BINARY_SOURCES`) | Remote server needed | No |
| Private ports | Overlay ports (`ports/clap`) | Local recipes | Any git repo |
| Extra tooling to install | None (bootstrapped from git) | Python | None |

**Decision:** vcpkg in manifest mode. No Python tool to install, the baseline
pins exact versions, the binary cache keeps CI fast without rebuilding the
dependencies on every commit, and overlay ports serve our own `clap` library.
In Part 2 the engine itself becomes a vcpkg port consumed with
`find_package(engine)`.

## Rendering, window and input: Raylib

| | Raylib | SFML | SDL2/3 |
|---|---|---|---|
| Scope | Window, 2D/3D, input, audio | Window, 2D, input, audio, network | Window, input, audio; 2D renderer basic |
| API | C, very small | C++ classes | C, larger |
| Dependencies | None beyond OpenGL/X11 | FreeType, OpenAL, several image libs | None |
| Letterboxing / render textures | Built in | Built in | Manual |
| Name clashes with `windows.h` | Yes (`CloseWindow`, `Rectangle`...) | No | No |

**Decision:** Raylib. It covers window, 2D rendering, input and audio with one
small dependency. Its known flaw, name clashes with `windows.h`, disappears
because `raylib.h` is included only inside the `graphics-raylib` plugin, which
the engine loads at runtime through backend-independent interfaces
(`IRenderer`, `IInput`). Switching to SFML or SDL means writing one new
plugin; neither the engine nor the game changes.

## Networking: POSIX sockets in our own wrapper

| | Raw sockets (wrapped) | Asio (standalone) | ENet | SFML Network |
|---|---|---|---|---|
| Extra dependency | None | Header-only library | Library | Ties networking to the graphics library |
| Learning value, control over threads | Full | Async model to learn | Reliability layer is a black box | Limited |
| Cross-platform | Needs a Winsock implementation | Yes | Yes | Yes |
| Server without a display | Yes | Yes | Yes | Links SFML anyway |

**Decision:** a small `UdpSocket` wrapper over POSIX sockets in
`engine/src/network/socket/`, with RAII ownership and engine-owned error codes
(`SocketError`). The protocol needs only `bind`, `sendto` and `recvfrom` on one
socket, so Asio's async machinery would be unused weight, and we control
exactly which thread touches the socket. The price is a missing Windows
implementation (see [Cross-platform support](#cross-platform-support)).
Socket headers never leave the private sources, so adding Winsock changes one
file.

### Transport: UDP only

| | TCP | UDP |
|---|---|---|
| Lost packet | Blocks every later packet until resent (head-of-line blocking) | Only that packet is lost |
| Stale data | Delivered late anyway | Dropped; the next snapshot replaces it |
| Message boundaries | Byte stream, framing needed | One datagram = one message |
| Reliability | Built in | Built where needed (retries, timeouts) |

**Decision:** UDP for everything, as the subject requires for gameplay. The
world is resent every tick, so a lost snapshot costs 16 ms; the connection is
retried until answered; heartbeats and timeouts detect lost peers. A second
TCP channel would add a second connection to manage for no benefit in
Part 1. Acknowledgement of events is planned (issue #86).

## Scripting: Lua 5.4 + sol2

| | Lua 5.4 + sol2 | LuaJIT | Python (pybind11) | No scripting |
|---|---|---|---|---|
| Embedding size | ~300 KB | ~500 KB | Full interpreter | — |
| Sandboxing | Choose libraries, custom environments | Same | Very hard | — |
| Speed | Good | Very fast | Slow | Fastest |
| Language version | 5.4 (integers, `<const>`) | 5.1 + extensions | — | — |
| C++ binding | sol2: modern, header-only | sol2 works | pybind11 | — |
| Used by game designers | Industry standard | Same | Rare in games | — |
| Iteration | Edit, restart server | Same | Same | Recompile |

**Decision:** Lua 5.4 through sol2. Lua is designed to be embedded, can be
sandboxed (we open only `base`, `math`, `string`, `table`, `coroutine` and
reject bytecode), and lets a designer add an enemy without touching C++. The
whole R-Type game is Lua; the engine has no game rule. LuaJIT is faster but
stuck on Lua 5.1 semantics and less maintained; our scripts are far from
being a bottleneck.

## Tests: GoogleTest

| | GoogleTest | Catch2 | doctest |
|---|---|---|---|
| Death tests, typed tests, matchers (gMock) | Yes | Partial | No |
| CTest integration (`gtest_discover_tests`) | Native | Yes | Yes |
| Compile time | Medium | Slow (v2) / medium (v3) | Fast |

**Decision:** GoogleTest, the most complete and best integrated with CTest. One
test executable per engine module, linked only against that module, so a test
that needs two modules reveals a coupling problem.

## Command line: clap

[clap](https://github.com/lucaspujol/clap) is a header-only C++20 argument
parser written by a team member, packaged as a vcpkg overlay port in
`ports/clap`. Compared with CLI11 or cxxopts it gives typed ranges
(`<ushort 1..65535>`), generated help with examples, and no code we do not
use. Being a vcpkg port, it follows the same rule as every other dependency:
no sources copied into the repository.

## Logging: our own logger

PROJECT.md first planned spdlog. We wrote a small logger in `engine-core`
instead (`log::debug/info/warn/error` with `std::format`, pluggable `Sink`s).
It is a few hundred lines including its documentation, needs no dependency, and its `MemorySink` lets
tests assert that a malformed packet was logged. spdlog's asynchronous sinks
and file rotation are not needed by a game server logging to stderr.

## Documentation: MkDocs

| | MkDocs Material | Sphinx | Docusaurus | Doxygen alone | Wiki |
|---|---|---|---|---|---|
| Source format | Markdown | reStructuredText (Markdown via plugin) | Markdown + React | Code comments | Web editor |
| Search, navigation, dark mode | Built in | Theme-dependent | Built in | Basic | Basic |
| Diagrams (Mermaid) | Built in | Plugin | Plugin | Graphviz | Varies |
| Lives in the repository, reviewed in pull requests | Yes | Yes | Yes | Yes | No |
| Toolchain | Python, one package | Python, several | Node.js | Doxygen | — |

**Decision:** Markdown in `docs/`, published with MkDocs Material to GitHub
Pages on every push to `main`, with the Doxygen API reference under `/api/`.
Docs are reviewed in the same pull request as the code they describe.

## Engine architecture and algorithms

### Entity Component System with sparse sets

| | Inheritance tree (`GameObject` → `Enemy` → `Bydo`) | Archetype ECS (EnTT groups, flecs) | Sparse-set ECS (ours) |
|---|---|---|---|
| Adding a behaviour to an existing type | Change the hierarchy | Add a component | Add a component |
| Iteration speed | Pointer chasing | Fastest | Fast (dense arrays) |
| Add / remove a component | — | Moves the entity between tables | O(1) |
| Implementation size | Small | Large | Small |

**Decision:** an ECS with one sparse set per component type
(`ComponentPool`): a sparse array maps the entity index to a slot in two dense
arrays (entities and values). Lookup, insertion and removal (swap with the
last element) are O(1); iteration walks a dense array. Archetypes iterate
faster but cost far more code; a shoot'em up has a few hundred entities.

Entity ids carry a **generation counter**: when an index is reused, its
generation increases, so a stale handle to a destroyed Bydo can never reach the
new entity that took its slot.

### Fixed-timestep loop

`FixedStepLoop` uses the accumulator pattern: real elapsed time from the clock
is added to an accumulator, and the simulation advances in fixed 1/60 s steps
while the accumulator holds at least one step, with a cap on steps per frame so a long pause cannot freeze the loop catching up. Game speed is therefore
independent of the frame rate and CPU speed, as the subject requires, and the
simulation is deterministic for a given input sequence. The clock is an
interface (`IClock`), so tests drive time with `ManualClock` and never sleep.

### Event bus (Mediator)

Subsystems never call each other: the network publishes `PlayerJoined`, the
server application subscribes and forwards it to Lua. The bus is typed
(one subscriber list per event type) and subscriptions are RAII objects that
unsubscribe themselves. This is the Mediator pattern the subject suggests.

### Thread communication: swap-and-drain queues

| | Lock-free queue | Mutex + vector, swap on drain (ours) |
|---|---|---|
| Complexity | High, subtle bugs | Very low |
| Cost per tick | Per-element atomics | One lock, one swap |
| Bounded | Fixed ring | Capacity check on push |

At 60 ticks per second with a few messages each, contention is negligible.
`drain()` swaps the whole buffer under the lock, so the game thread holds it
for a few nanoseconds. ThreadSanitizer runs on every pull request.

### Sequence numbers with wrap-around

Packet sequences are 32-bit counters that wrap. `is_newer_sequence(a, b)` uses
serial-number arithmetic (as in RFC 1982): `a` is newer when it is ahead of
`b` by less than half the range. Old, duplicated and reordered inputs and
snapshots are rejected correctly even after a wrap.

### Custom algorithms

| Algorithm | Why we needed it |
|---|---|
| Validate-then-allocate decoding (`ByteReader`) | A packet announcing 65 535 entities in 13 bytes must be rejected before any allocation |
| Snapshot splitting | A world larger than 62 entities is split into several packets with the same tick, each below the 1200-byte MTU-safe limit; the client merges them by entity id |
| Lowest-free player id / slot | Ids and colours are reused when a player leaves, so four simultaneous players always get four distinct slots |
| AABB overlap in `engine-physics` | Every pair of hitboxes is tested in C++ each tick (O(n²), fine for a few hundred entities; a broad phase is the next step), then filtered by `collides_with` tags before reaching Lua |

## Storage

What the project stores, and where:

| Data | Storage | Persistence |
|---|---|---|
| Game rules and tunables | Lua files in `scripts/` | Files, versioned in git |
| Sprites | PNG files in `assets/` | Files, versioned in git |
| World state | In memory, server only | Lost when the server stops (by design: a round lasts minutes) |
| Sessions | In memory | Expire after 5 s of silence |
| Logs | stderr | Redirect to a file if needed |

Part 1 has no data that must survive a restart. For Part 2 features (global
scoreboard, accounts), we compared:

| | Flat files (JSON / Lua) | SQLite | PostgreSQL | Redis |
|---|---|---|---|---|
| Deployment | Nothing | One library, one file | A server to run | A server to run |
| Concurrent writes | Unsafe without locking | Serialized, safe | Safe | Safe |
| Crash safety | Partial writes possible | Atomic transactions, journal | Full | Configurable |
| Queries (top 10 scores) | Load everything | SQL | SQL | Sorted sets |
| Fits a single game server | Yes | Yes | Oversized | Oversized |

**Decision for Part 2:** SQLite through vcpkg if persistence is needed. It
gives atomic, crash-safe writes in a single file with no server to deploy,
and it fits one game server. Files stay the format for content (scripts,
assets, levels) because designers edit them by hand and git versions them.

## Security

### Network

Every packet is untrusted. The protocol's
[Security Considerations](protocol.md#security-considerations) list each
threat and its mitigation: bounds-checked decoding, exact size validation,
allocation only after validation, bounded queues, a session limit, inputs
instead of positions, and sequence checks. The main accepted risk is the lack
of authentication: someone able to spoof a player's address can inject
inputs. Encryption and authentication (for example a session token in
`connect_accept`) are Part 2 work.

### Scripting

| Risk | Mitigation |
|---|---|
| Scripts reading files or running commands | `io`, `os`, `package`, `dofile`, `loadfile` are not available |
| Malicious precompiled bytecode (can crash the VM) | Only text chunks are loaded |
| `require` escaping the scripts folder | Module names may only contain letters, digits, `_` and `.`, resolved under `lib/` |
| A buggy script stopping the server | Every call into Lua is protected; errors are logged with file and line |

### Code and dependencies

| Technology | Main vulnerability class | What we do |
|---|---|---|
| C++ | Memory errors, data races, undefined behaviour | AddressSanitizer + UBSan and ThreadSanitizer on every pull request, clang-tidy with warnings as errors, compiler warnings as errors in CI, RAII everywhere |
| Lua | Sandbox escape | See above |
| Raylib (stb_image) | Malformed images | Assets ship with the game; nothing is loaded from the network |
| vcpkg dependencies | Vulnerable versions | Versions pinned by `builtin-baseline`, updated deliberately in a pull request |
| GitHub Actions | Compromised or outdated actions | Dependabot opens weekly update pull requests |

### Long-term monitoring

- CI runs the sanitizers, clang-tidy and every test on each pull request;
  nothing merges red.
- Dependabot watches the CI actions; the vcpkg baseline is bumped on purpose
  and reviewed like code, and GitHub security advisories cover the
  repository.
- Malformed packets are logged with their sender, and the server logs a
  traffic summary (rate, loss, reordering) every 10 seconds, so floods and
  abuse are visible in the logs.

## Cross-platform support

The subject requires Linux and recommends Windows; macOS does not count as
cross-platform.

| Platform | State |
|---|---|
| Linux | Supported, tested in CI with GCC and Clang |
| macOS | Supported, tested in CI with Apple Clang |
| Windows | Not supported yet |

What is already portable: CMake presets, vcpkg, the export macros
(`__declspec(dllexport)` on Windows), `DynamicLibrary` (`LoadLibraryW`),
Raylib. What is missing: a Winsock implementation of `UdpSocket` and a Windows
CI job. We postponed it to finish the gameplay first; because sockets are
confined to one private file behind an engine-owned API, it is a contained
task.
