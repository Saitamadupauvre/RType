# Getting started

This page takes you from a fresh machine to a running game. The
[README](https://github.com/Saitamadupauvre/RType#readme) has the same steps in
short form.

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
dependency listed in `vcpkg.json` (Raylib, Lua, sol2, GoogleTest, clap) is
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
git clone https://github.com/Saitamadupauvre/RType.git r-type
cd r-type
cmake --preset release
cmake --build --preset release
```

The first configure builds every dependency and takes a few minutes; later
ones reuse the vcpkg cache.

Result, in `build/release/bin/`:

```
bin/
├── r-type_server
├── r-type_client
├── libengine-core.so        (.dylib on macOS)
├── libengine-graphics.so
├── libengine-network.so
├── libengine-physics.so
├── libengine-script.so
├── libengine-audio.so
├── plugins/graphics-raylib.so
├── plugins/audio-raylib.so
├── scripts/                 the Lua game
└── assets/                  sprites, sounds, music
```

## 3. Launch

Start the server, then one client per player:

```sh
./build/release/bin/r-type_server 4242
./build/release/bin/r-type_client 127.0.0.1 4242
```

Or, without a separate server:

```sh
./build/release/bin/r-type_client --host 4242   # host a game others can join, and play on it
./build/release/bin/r-type_client --solo        # play alone, no network
```

| Command | Effect |
|---|---|
| `r-type_server <port>` | Listens on UDP `<port>` for up to 4 players |
| `r-type_server <port> --max-players 2` | Refuses a third player |
| `r-type_client <server-ip> <port>` | Opens a 1280 × 720 window and joins the server |
| `r-type_client <server-ip> <port> --music-volume 30 --effects-volume 80` | Same, with quieter sounds (`0`–`100`) |
| `r-type_client <server-ip> <port> --mute` | Same, muted (`M` toggles it in game) |
| `r-type_client --host <port>` | Hosts a game on UDP `<port>` for up to 4 players and joins it; others connect with `r-type_client <your-ip> <port>` |
| `r-type_client --solo` | Plays alone: server and client in one process, no socket opened |
| `--help` on either | Prints the usage |

Playing over a network: start the server on one machine, open its UDP port in
the firewall, and start each client with the server's IPv4 address.

The server logs every connection, disconnection and dropped packet on
standard error, and a traffic summary every 10 seconds. Stop it with
`Ctrl+C`: it tells every client it is shutting down.

## 4. Install a standalone copy

```sh
cmake --install build/release --prefix dist
```

`dist/bin/` is self-contained: copy or zip the whole folder. The executables
find their files relative to their own location, never the working directory.

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `Could not find toolchain file: /scripts/buildsystems/vcpkg.cmake` | `VCPKG_ROOT` is not set in this shell. Export it and configure again. |
| A vcpkg port fails with `autoconf`/`libtool` not found | Install the system packages of step 1. |
| Raylib fails to build with missing `X11/...` headers | Install the X11 development packages of step 1. |
| `cannot listen on port 4242: Address already in use` (exit `84`) | Another server uses the port. Pick another one or stop it. |
| Client prints an error about `graphics-raylib` (exit `84`) | The `plugins/` folder is not next to the executable. Run the client from `build/<preset>/bin/`, without moving it. |
| Client opens but shows only stars | It is not connected: wrong address or port, server not started, server full, or UDP blocked by a firewall. Check the server logs. |
| Your ship stops responding after a while | The server stopped or became unreachable; the client gives up after 5 seconds. Restart the client. |
| Stale build after pulling | `rm -rf build/<preset>` and build again. |
