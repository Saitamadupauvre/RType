# RType

## Requirements

- CMake 3.25+, a C++20 compiler (GCC, Clang or MSVC), Ninja (optional).
- [vcpkg](https://github.com/microsoft/vcpkg): every dependency comes from
  `vcpkg.json` and is built automatically on the first configure.

```sh
git clone https://github.com/microsoft/vcpkg ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh              # bootstrap-vcpkg.bat on Windows
export VCPKG_ROOT=~/vcpkg               # add it to your shell profile
```

On Linux, raylib's vcpkg dependencies need build tools and X11 headers from
the system:

```sh
sudo apt install autoconf autoconf-archive automake libtool pkg-config \
    libxinerama-dev libxcursor-dev libxrandr-dev libxi-dev xorg-dev libglu1-mesa-dev
sudo pacman -S --needed autoconf autoconf-archive automake libtool pkgconf \
    libxinerama libxcursor libxrandr libxi mesa
```

## Build

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

`ninja-debug` / `ninja-release` presets use Ninja (on Windows, run them from a
Developer Command Prompt). CI runs `cmake --workflow --preset ci`.

## Run

`cmake --build --preset <preset>` puts both executables in `build/<preset>/bin/`,
next to the engine libraries and the `plugins/` folder. They find their files
relative to the executable, so they can be started from any directory.

```sh
./build/debug/bin/r-type_server <port> [--max-players <n>]
./build/debug/bin/r-type_client <server-ip> <port>
```

| Executable      | Argument    | Meaning                                       |
|-----------------|-------------|-----------------------------------------------|
| `r-type_server` | `port`      | UDP port to listen on, `1`–`65535`            |
| `r-type_server` | `-p, --max-players` | Maximum connected players, `1`–`4`, default `4` |
| `r-type_client` | `server-ip` | Address of the server, e.g. `127.0.0.1`       |
| `r-type_client` | `port`      | UDP port of the server, `1`–`65535`           |
| both            | `-h, --help`| Print the usage and exit with code `0`        |

A missing or invalid argument prints the error and the usage, and exits with
code `84`.

A local game with one server and two clients, each in its own terminal:

```sh
./build/debug/bin/r-type_server 4242
./build/debug/bin/r-type_client 127.0.0.1 4242
./build/debug/bin/r-type_client 127.0.0.1 4242
```

The client loads the graphics plugin from the `plugins/` folder next to its
executable and exits with code `84` if the plugin is missing or the window
cannot open.

## Checks (Linux / macOS)

```sh
cmake --workflow --preset asan       # AddressSanitizer + UndefinedBehaviorSanitizer
cmake --workflow --preset tidy       # clang-tidy
```

**Formatting:** CI requires `clang-format 23.1.1`. Install with:
```sh
pipx install clang-format==23.1.1
```
