# Commands

Every command used to build, run, test and check the project. All of them
need `VCPKG_ROOT` (see [Getting started](getting-started.md)).

## Presets

Defined in `CMakePresets.json`. The build folder is always `build/<preset>/`.

| Preset | Generator | Build type | Purpose |
|---|---|---|---|
| `debug` | Default | Debug | Day-to-day development |
| `release` | Default | Release | Playing |
| `ninja-debug` | Ninja | Debug | Faster incremental builds |
| `ninja-release` | Ninja | Release | Faster release builds |
| `ci` | Default | Release | What CI builds: warnings are errors |
| `asan` | Default | Debug | AddressSanitizer + UndefinedBehaviorSanitizer |
| `tsan` | Default | Debug | ThreadSanitizer |
| `tidy` | Default | Debug | clang-tidy on every file, warnings are errors |

## Build

```sh
cmake --preset ninja-debug                       # configure
cmake --build --preset ninja-debug               # build everything
cmake --build --preset ninja-debug --target r-type_server   # one target
```

Useful targets: `r-type_server`, `r-type_client`, `r-type_sandbox`,
`graphics-raylib`, `audio-raylib`, `engine-<module>`, `engine_<module>_tests`, `docs`
(Doxygen, if installed), `format`, `format-check`.

CMake options:

| Option | Default | Effect |
|---|---|---|
| `RTYPE_BUILD_SANDBOX` | `ON` | Build `r-type_sandbox` |
| `RTYPE_WARNINGS_AS_ERRORS` | `OFF` (`ON` in `ci`) | Warnings fail the build in game code |
| `ENGINE_WARNINGS_AS_ERRORS` | `OFF` (`ON` in `ci`) | Same for engine code |

## Run

```sh
./build/ninja-debug/bin/r-type_server 4242
./build/ninja-debug/bin/r-type_client 127.0.0.1 4242
./build/ninja-debug/bin/r-type_client 127.0.0.1 4242 --music-volume 30 --mute
./build/ninja-debug/bin/r-type_client --host 4242   # server thread + client
./build/ninja-debug/bin/r-type_client --solo        # no socket
./build/ninja-debug/bin/r-type_sandbox           # tries the graphics plugin alone
```

## Install

```sh
cmake --install build/release --prefix dist
```

## Tests

```sh
ctest --preset ninja-debug                       # every test
ctest --preset ninja-debug -R ProtocolHandler    # tests whose name matches
ctest --preset ninja-debug --output-on-failure
./build/ninja-debug/bin/engine_network_tests   # one module's executable directly
./build/ninja-debug/bin/engine_network_tests --gtest_filter='SnapshotReceiver.*'
```

There is one test executable per engine module (`engine_<module>_tests`), per
game target and per plugin.

## Workflows

A workflow runs configure, build and test in one command.

| Workflow | Steps |
|---|---|
| `cmake --workflow --preset ci` | Configure, build, test with warnings as errors (what CI runs) |
| `cmake --workflow --preset asan` | Same with AddressSanitizer + UBSan |
| `cmake --workflow --preset tsan` | Same with ThreadSanitizer |
| `cmake --workflow --preset tidy` | Configure and build with clang-tidy |

## Formatting

CI requires **clang-format 23.1.1** exactly:

```sh
pipx install clang-format==23.1.1
cmake --build --preset ninja-debug --target format         # format in place
cmake --build --preset ninja-debug --target format-check   # fail if not formatted
```

## `fred.sh`

A shortcut script at the repository root:

| Command | Does |
|---|---|
| `./fred.sh run` | Configure and build `release` |
| `./fred.sh debug` | Configure and build `debug` |
| `./fred.sh tests` | Build `debug` and run the tests |
| `./fred.sh tidy` | clang-tidy workflow |
| `./fred.sh format` | Format the sources in place |
| `./fred.sh all` | Format check, debug build, tests and clang-tidy: run it before pushing |

## Documentation

```sh
pip install "mkdocs==1.6.1" "mkdocs-material==9.7.7"
mkdocs serve                                     # live preview on http://127.0.0.1:8000
mkdocs build                                     # static site in site/
cmake --build --preset release --target docs     # Doxygen API reference in build/release/docs/html
```

## Common situations

| Situation | Commands |
|---|---|
| After pulling | `cmake --build --preset ninja-debug && ctest --preset ninja-debug` |
| After changing `vcpkg.json` | `cmake --preset ninja-debug` (vcpkg installs the new port) |
| Strange build errors | `rm -rf build/ninja-debug`, configure and build again |
| Before pushing | `./fred.sh all` |
| After editing a Lua script | Rebuild (scripts are copied next to the executables), re-sync the test fixture (see [Writing the game](writing-the-game.md#tests)) |
