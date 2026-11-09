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
cmake --build --preset ninja-debug --target editor   # one target
```

Useful targets: `editor`, `engine-runtime`, `graphics-raylib`, `audio-raylib`, `engine-<module>`, `engine_<module>_tests`, `docs`
(Doxygen, if installed), `format`, `format-check`.

CMake options:

| Option | Default | Effect |
|---|---|---|
| `WORKSPACE_BUILD_TESTS` | `ON` at the top level | Build the GoogleTest suites |
| `WORKSPACE_WARNINGS_AS_ERRORS` | `OFF` (`ON` in `ci`) | Warnings fail the build in the editor, the runtime and the tests |
| `ENGINE_WARNINGS_AS_ERRORS` | `OFF` (`ON` in `ci`) | Same for engine code |

## Run

```sh
./build/ninja-debug/bin/editor examples/r-type                               # the editor
./build/ninja-debug/bin/engine-runtime --game examples/r-type --solo         # no socket
./build/ninja-debug/bin/engine-runtime --game examples/r-type --host 4242    # server thread + client
./build/ninja-debug/bin/engine-runtime --game examples/r-type 127.0.0.1 4242 --music-volume 30 --mute
./build/ninja-debug/bin/engine-runtime --game examples/r-type --server 4242  # dedicated server
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
plugin (`plugin_<name>_tests`), one for the editor (`editor_core_tests`) and
one for the runtime (`runtime_cli_tests`).

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

A shortcut script in `scripts/`:

| Command | Does |
|---|---|
| `scripts/fred.sh run` | Configure and build `release` |
| `scripts/fred.sh editor [folder]` | Build `release` and open the editor on `folder` (`examples/r-type` by default) |
| `scripts/fred.sh play <mode>` | Build `release` and run `examples/r-type` with the runtime: `play --solo`, `play --host 4242`, `play 127.0.0.1 4242`, `play --server 4242` |
| `scripts/fred.sh debug` | Configure and build `debug` |
| `scripts/fred.sh tests` | Build `debug` and run the tests |
| `scripts/fred.sh tidy` | clang-tidy workflow |
| `scripts/fred.sh format` | Format the sources in place |
| `scripts/fred.sh all` | Format check, debug build, tests and clang-tidy: run it before pushing |

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
| Before pushing | `scripts/fred.sh all` |
| After editing a game's Lua script | Nothing to build: press Play in the editor, or restart the runtime |
