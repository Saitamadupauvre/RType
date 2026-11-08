# Getting started

This page takes you from a fresh machine to the editor open on the example
game. The [README](https://github.com/Saitamadupauvre/RType#readme) has the
same steps in short form.

## 1. Requirements

| Tool | Notes |
|---|---|
| C++20 compiler | GCC or Clang on Linux, Apple Clang on macOS |
| CMake 3.25+ | |
| Ninja | Optional, used by the `ninja-*` presets |
| Git, curl, zip, unzip, tar | Used by vcpkg to fetch and unpack sources |
| [vcpkg](https://github.com/microsoft/vcpkg) | Builds every third-party library |

Windows is not supported yet (see
[Comparative study](comparative-study.md#cross-platform-support)).

### vcpkg

```sh
git clone https://github.com/microsoft/vcpkg ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh -disableMetrics
echo 'export VCPKG_ROOT=~/vcpkg' >> ~/.bashrc     # or ~/.zshrc
export VCPKG_ROOT=~/vcpkg
```

The CMake presets read `VCPKG_ROOT` to find the vcpkg toolchain. Every
dependency listed in `vcpkg.json` (Raylib, Lua, sol2, Clay, nlohmann-json, GoogleTest, clap) is
then built inside the build folder: nothing is installed system-wide.

### System packages (Linux)

Raylib needs the X11 and OpenGL development headers, and some vcpkg ports need
the autotools.

=== "Debian / Ubuntu"

    ```sh
    sudo apt install build-essential cmake ninja-build git curl zip unzip tar \
        autoconf autoconf-archive automake libtool pkg-config \
        libxinerama-dev libxcursor-dev libxrandr-dev libxi-dev xorg-dev libglu1-mesa-dev
    ```

=== "Arch Linux"

    ```sh
    sudo pacman -S --needed base-devel cmake ninja git curl zip unzip tar \
        autoconf autoconf-archive automake libtool pkgconf \
        libxinerama libxcursor libxrandr libxi mesa
    ```

=== "macOS"

    ```sh
    xcode-select --install
    brew install cmake ninja
    ```

## 2. Build

```sh
git clone https://github.com/Saitamadupauvre/RType.git engine
cd engine
cmake --preset release
cmake --build --preset release
```

The first configure builds every dependency and takes a few minutes; later
ones reuse the vcpkg cache.

Result, in `build/release/bin/`:

```
bin/
├── editor                   the editor
├── editor-assets/fonts/     the editor's fonts
├── engine-runtime           runs any game folder
├── libengine-*.so           engine libraries (.dylib on macOS)
└── plugins/
    ├── graphics-raylib.so   window, drawing, input
    └── audio-raylib.so      sound and music
```

This is built once. Games are not compiled: they are folders the editor and
the runtime read (see [Making games](guide/index.md)).

## 3. Open the example in the editor

```sh
./build/release/bin/editor examples/r-type
```

The editor opens the R-Type example on its start scene. Press **Play** in the
menu bar: the game opens in its own window. Move with the arrow keys, fire
with `Space`, close the window to come back to the editor.

Next: the [tour of the editor](guide/editor-tour.md), then
[Your first game](guide/first-game.md).

## 4. Run a game without the editor

```sh
./build/release/bin/engine-runtime --game examples/r-type --solo         # play alone
./build/release/bin/engine-runtime --game examples/r-type --host 4242    # host, others join
./build/release/bin/engine-runtime --game examples/r-type 127.0.0.1 4242 # join
./build/release/bin/engine-runtime --game examples/r-type --server 4242  # dedicated server
```

Every option is in [Run and export](guide/run-and-export.md). The server logs
every connection, disconnection and dropped packet on standard error, and a
traffic summary every 10 seconds. Stop it with `Ctrl+C`: it tells every client
it is shutting down.

`scripts/fred.sh editor` and `scripts/fred.sh play --solo` build the release
preset and do the same.

## 5. Install a standalone copy

```sh
cmake --install build/release --prefix dist
```

`dist/bin/` holds the editor, the runtime, the engine libraries and the
plugins; `dist/examples/` holds the example games. The executables find their
files relative to their own location, never the working directory: copy or zip
the whole folder.

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `Could not find toolchain file: /scripts/buildsystems/vcpkg.cmake` | `VCPKG_ROOT` is not set in this shell. Export it and configure again. |
| A vcpkg port fails with `autoconf`/`libtool` not found | Install the system packages of step 1. |
| Raylib fails to build with missing `X11/...` headers | Install the X11 development packages of step 1. |
| `cannot listen on port 4242: Address already in use` (exit `84`) | Another server uses the port. Pick another one or stop it. |
| The runtime prints an error about `graphics-raylib` (exit `84`) | The `plugins/` folder is not next to the executable. Run it from `build/<preset>/bin/`, without moving it alone. |
| `editor: no game.json in ...` | The folder given to the editor is not a game folder. Point it at a folder holding `game.json`, such as `examples/r-type`. |
| Play does nothing but print an error in the console | `engine-runtime` is not next to the editor. Build the whole project, or pass `--runtime <program>`. |
| A joining client stays on the loading screen or an empty game | It is not connected: wrong address or port, server not started, server full, or UDP blocked by a firewall. Check the server logs. |
| The game stops responding after a while | The server stopped or became unreachable; the client gives up after 5 seconds. Restart it. |
| Stale build after pulling | `rm -rf build/<preset>` and build again. |
