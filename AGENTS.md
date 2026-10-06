# AGENTS.md

Instructions for anyone (human or AI agent) writing code in this repository.
Read [docs/PROJECT.md](docs/PROJECT.md) first: it is the reference for the
architecture, the Lua API and the roadmap.

## Golden rules

1. **Every feature and every bug fix comes with GoogleTest tests.** No pull
   request without tests. A bug fix starts with a test that reproduces it.
2. **The engine never knows about the game.** Nothing under `engine/` includes,
   links or names anything from `game/`. Before adding something to the engine,
   ask: *would this make sense in a platformer?* If not, it belongs in Lua.
3. **The engine stays extractable.** `engine/` only uses its own CMake helpers
   (`engine/cmake/`) and variables, never root ones. The game only links
   `engine::*` targets. In Part 2 `engine/` moves to its own repository and is
   consumed through vcpkg with `find_package(engine)`.
4. **Dependencies only through vcpkg.** Add a port to `vcpkg.json` in the same
   pull request as the module that uses it. Never copy library sources, never
   use `FetchContent` or git submodules for third-party code.
5. **CI must be green** (Linux, Windows, macOS, sanitizers, clang-tidy, format)
   before merging.

## Repository layout

```
engine/                     C++ engine, shared libraries engine-<module>
  cmake/                    engine_add_module / engine_add_plugin helpers
  include/engine/<module>/  public headers
  src/<module>/             implementation and private headers
plugins/<name>/             runtime plugins (bin/plugins/), e.g. graphics-raylib
game/server, game/client    r-type_server / r-type_client entry points
game/scripts/               Lua game code (main.lua is the entry point)
assets/                     sprites, sounds, music
tests/engine/<module>/      tests of one engine module
tests/game/<target>/        tests of one game target (server, client)
tests/fixtures/             test data: packets, Lua scripts...
docs/                       documentation (English): engine.md, plugins.md, game.md
```

## Tests

- Tests mirror the code: a feature in `engine/src/<module>/Foo.cpp` is tested in
  `tests/engine/<module>/FooTest.cpp`, registered in `tests/CMakeLists.txt`
  with `engine_add_test(<module> SOURCES ...)`.
- One test executable per module (`engine_<module>_tests`), linked **only**
  against that module. Needing another module in a test means the modules are
  coupled: fix the design, not the test.
- Game code under test lives in a static library linked by its executable
  (e.g. `r-type_server_cli`), tested in `tests/game/<target>/` and registered
  with `game_add_test(<target> SOURCES ... LINKS <library>)`.
- Test names: `TEST(Feature, ExpectedBehaviour)`, e.g.
  `TEST(PacketParser, RejectsTruncatedHeader)`.
- Test the failure paths, not only the happy path: truncated or random packets,
  Lua scripts with errors, duplicate or reserved names, missing plugin, missing
  symbol, wrong plugin API version.
- Test data goes in `tests/fixtures/` and is reached through the
  `RTYPE_FIXTURES_DIR` macro, never through the working directory.
- Tests must be deterministic: no real network, no sleeps, no display. Inject
  the clock and random seeds.

## Engine rules

- Every engine module is a shared library created with `engine_add_module`.
  Symbols are hidden by default: mark public API with the generated
  `ENGINE_<MODULE>_EXPORT` macro from `<engine/<module>/Export.hpp>`.
- Third-party headers never appear in public headers (`engine/include/`).
  `raylib.h` is included only in `plugins/graphics-raylib/`; Asio only in
  `.cpp` files of `engine/src/network/`. Link third-party libraries with
  `PRIVATE_DEPENDS`.
- No global state in headers (no `static` variables in templates): registries
  live in a `.cpp` of `engine-core` behind exported functions.
- Subsystems talk through the ECS and the event bus, never by calling each
  other directly.
- Graphics calls happen on the main thread only. Other threads push data into
  queues.
- Game logic never depends on frame rate: use the fixed-timestep loop and the
  engine clock.
- Files are found relative to the executable, never the working directory.
- Every packet and every Lua call is untrusted: validate, catch, log, and keep
  running.

## Comments and documentation

- **No comments, under any circumstance.** No `//`, `/* */` or `#` comments
  explaining code, no commented-out code, no TODOs in the source (open an
  issue instead). Code must explain itself through names and small functions.
- **The only exception is professional Doxygen**, and only where it is
  important: public API in `engine/include/` and plugin interfaces. Use
  `/** ... */` blocks with `@brief`, `@param`, `@return`, `@throws` and
  `@note` as needed. Document the contract (what, preconditions, errors,
  thread safety), never the implementation.
- Private code, `.cpp` files, tests, CMake and CI files get no comments.
- Closing namespace comments (`} // namespace engine::core`) are allowed:
  clang-format adds them automatically.

## Code style

- C++20. Formatting follows `.clang-format`, checks follow `.clang-tidy`.
- `snake_case` for functions and variables, `PascalCase` for types and files,
  namespaces `engine::<module>`.
- Changes to the Lua API update docs/PROJECT.md §10 in the same pull request.

## Commands

```sh
cmake --preset ninja-debug              # needs VCPKG_ROOT (see README)
cmake --build --preset ninja-debug
ctest --preset ninja-debug
cmake --workflow --preset ci            # what CI runs
cmake --workflow --preset asan          # AddressSanitizer + UBSan
cmake --workflow --preset tidy          # clang-tidy
cmake --build --preset ninja-debug --target format
```

## Git workflow

- One issue → one branch → one pull request targeting `dev` (`Closes #<n>`).
  `dev` is merged into `main` for releases.
- Commits follow Conventional Commits (`feat:`, `fix:`, `test:`, `docs:`,
  `build:`, `ci:`, `refactor:`), one logical change per commit.
- Run the tests and the format target before pushing.
