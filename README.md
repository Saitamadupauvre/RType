# R-Type

A networked multiplayer remake of the classic horizontal shoot'em up **R-Type**,
built on our own C++20 game engine. Up to four players connect to an
authoritative server over UDP and fight waves of Bydos together. The gameplay
is written in Lua; the engine knows nothing about R-Type.

Epitech *Advanced C++* project (B-CPP-500).

- **Full documentation:** [`docs/`](docs/index.md), published online with
  MkDocs (see [Documentation](#documentation)).
- **Protocol specification:** [`docs/protocol.md`](docs/protocol.md).

## Contents

- [Features](#features)
- [Supported platforms](#supported-platforms)
- [Requirements](#requirements)
- [Build](#build)
- [Launch the game](#launch-the-game)
- [How to play](#how-to-play)
- [Install](#install)
- [Tests and checks](#tests-and-checks)
- [Repository layout](#repository-layout)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [Authors](#authors)
- [License](#license)

## Features

- Authoritative, multithreaded server (receive thread, game thread, send
  thread) running the game at a fixed 60 ticks per second.
- Custom binary protocol over UDP, validated before every read, documented
  RFC-style.
- Up to 4 players, each with their own colour.
- Dedicated server, hosted game (`--host`: play and let others join) or solo
  game (`--solo`: no network at all).
- Bydos spawning at random heights on the
  right and shooting back, missiles, collisions detected by the engine's
  physics module, game over and automatic restart.
- Looping music, shot and explosion sounds, with separate music and effects
  volumes and a mute key.
- A crashed or disconnected client is dropped after 5 seconds and the other
  players are notified; malformed packets are dropped and logged.
- Engine split into shared libraries (`core`, `script`, `network`,
  `physics`, `graphics`, `audio`), with the Raylib graphics and audio backends
  loaded at runtime as plugins.
- Gameplay written in sandboxed Lua 5.4 scripts.

## Supported platforms

| Platform | Server | Client | Tested in CI |
|---|---|---|---|
| Linux (GCC, Clang) | Yes | Yes | Yes |
| macOS (Apple Clang) | Yes | Yes | Yes |
| Windows | No | No | No |

Windows support is postponed: the network module only has a POSIX socket
implementation for now. See the
[comparative study](docs/comparative-study.md#cross-platform-support).

## Requirements

| Tool | Version | Notes |
|---|---|---|
| C++ compiler | Recent GCC, Clang or Apple Clang | C++20; CI uses the compilers of `ubuntu-latest` and `macos-latest` |
| CMake | 3.25+ | |
| Ninja | any | Optional; used by the `ninja-*` presets |
| Git | any | Needed by vcpkg |
| [vcpkg](https://github.com/microsoft/vcpkg) | any recent | Builds every third-party library |

Every third-party library (Raylib, Lua, sol2, GoogleTest, and
[`clap`](https://github.com/lucaspujol/clap), our command-line parser served
from the overlay port in `ports/`) comes from [`vcpkg.json`](vcpkg.json) and is built automatically the first
time you configure. Nothing has to be installed system-wide except the
compiler toolchain and, on Linux, the X11/OpenGL headers Raylib needs.

### 1. Install vcpkg

```sh
git clone https://github.com/microsoft/vcpkg ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh -disableMetrics
export VCPKG_ROOT=~/vcpkg               # add this line to your shell profile
```

### 2. Install the system packages (Linux only)

Debian / Ubuntu:

```sh
sudo apt install build-essential cmake ninja-build git curl zip unzip tar \
    autoconf autoconf-archive automake libtool pkg-config \
    libxinerama-dev libxcursor-dev libxrandr-dev libxi-dev xorg-dev libglu1-mesa-dev
```

Arch Linux:

```sh
sudo pacman -S --needed base-devel cmake ninja git curl zip unzip tar \
    autoconf autoconf-archive automake libtool pkgconf \
    libxinerama libxcursor libxrandr libxi mesa
```

On macOS, the Xcode Command Line Tools (`xcode-select --install`) plus
`brew install cmake ninja` are enough.

## Build

```sh
git clone https://github.com/Saitamadupauvre/RType.git r-type
cd r-type
cmake --preset release            # first run builds the dependencies: a few minutes
cmake --build --preset release
```

Both executables end up in `build/release/bin/`, next to the engine libraries,
the `plugins/` folder, the Lua `scripts/` and the `assets/`.

| Preset | Generator | Use |
|---|---|---|
| `release` / `debug` | Default generator | Playing / debugging |
| `ninja-release` / `ninja-debug` | Ninja | Faster builds |
| `asan`, `tsan`, `tidy`, `ci` | Default generator | Checks, see [Tests and checks](#tests-and-checks) |

The output folder is always `build/<preset>/`. The rest of this README uses
`release`; replace it with the preset you built.

## Launch the game

A game is **one server** and **one to four clients**. Start the server first,
then one client per player, each in its own terminal.

### On one machine

```sh
# terminal 1: the server, listening on UDP port 4242
./build/release/bin/r-type_server 4242

# terminals 2 and 3: two players
./build/release/bin/r-type_client 127.0.0.1 4242
./build/release/bin/r-type_client 127.0.0.1 4242
```

The game starts as soon as the first player connects. Players who join later
get a ship immediately.

Without a separate server:

```sh
./build/release/bin/r-type_client --host 4242   # host on UDP 4242 and play; others join with <your-ip> 4242
./build/release/bin/r-type_client --solo        # play alone, no socket
```

### Over a local network

1. On the server machine, find its IPv4 address (`ip addr` on Linux,
   `ipconfig getifaddr en0` on macOS) and start the server:
   `./build/release/bin/r-type_server 4242`.
2. Allow incoming **UDP** traffic on that port in its firewall.
3. On each player's machine: `./build/release/bin/r-type_client <server-ip> 4242`.

### Command line

```
r-type_server [-h] [-p <1..4>] <port>
r-type_client [-h] [--music-volume <0..100>] [--effects-volume <0..100>] [--mute] <server-ip> <port>
r-type_client [-h] [options] --host <port>
r-type_client [-h] [options] --solo
```

| Executable | Argument | Meaning |
|---|---|---|
| `r-type_server` | `port` | UDP port to listen on, `1`–`65535` |
| `r-type_server` | `-p`, `--max-players` | Maximum connected players, `1`–`4`, default `4` |
| `r-type_client` | `server-ip` | IPv4 address of the server, e.g. `127.0.0.1` |
| `r-type_client` | `port` | UDP port of the server, `1`–`65535` |
| `r-type_client` | `--music-volume` | Music volume, `0`–`100`, default `100` |
| `r-type_client` | `--effects-volume` | Sound effects volume, `0`–`100`, default `100` |
| `r-type_client` | `--mute` | Start muted (`M` toggles it in game) |
| both | `-h`, `--help` | Print the usage and exit with code `0` |

### Exit codes and errors

| Situation | Behaviour |
|---|---|
| Missing or invalid argument | Prints the error and the usage, exits with `84` |
| Server cannot listen on the port (already in use...) | Exits with `84` |
| Client cannot load `plugins/graphics-raylib` or open the window | Exits with `84` |
| Client cannot load `plugins/audio-raylib` or finds no audio device | Prints a warning and plays without sound |
| Server full (`--max-players` reached) | The client is refused and stays on an empty screen; close it |
| Server stops or stops answering | The client stops receiving updates after 5 seconds; close it |
| `Ctrl+C` / SIGTERM on the server | Notifies every client, exits with `0` |

The executables find their libraries, plugin, scripts and assets **relative to
their own location**, so they can be started from any directory. Do not move
a single executable out of `bin/`: move the whole folder.

## How to play

| Action | Keys |
|---|---|
| Move | Arrow keys, or `Z` `Q` `S` `D` |
| Fire | `Space` (hold to keep firing) |
| Mute / unmute | `M` |
| Quit | Close the window |

- Shoot the Bydos coming from the right. A Bydo touching your ship destroys
  it.
- Each player has their own colour (blue, red, green, yellow, by join order).
- When every ship is destroyed, the game is over and restarts after 3
  seconds with a ship for every connected player.

More in [docs/how-to-play.md](docs/how-to-play.md).

## Install

To get a self-contained folder you can copy or zip:

```sh
cmake --install build/release --prefix dist
./dist/bin/r-type_server 4242
```

`dist/bin/` contains the executables, the engine libraries, `plugins/`,
`scripts/` and `assets/`.

## Tests and checks

```sh
cmake --preset ninja-debug
cmake --build --preset ninja-debug
ctest --preset ninja-debug              # every GoogleTest suite

cmake --workflow --preset ci            # what CI runs: configure, build, test
cmake --workflow --preset asan          # AddressSanitizer + UndefinedBehaviorSanitizer
cmake --workflow --preset tsan          # ThreadSanitizer
cmake --workflow --preset tidy          # clang-tidy, warnings as errors
cmake --build --preset ninja-debug --target format   # clang-format 23.1.1
```

`./fred.sh all` runs the format check, the debug build, the tests and
clang-tidy in one go. Every command is described in
[docs/commands.md](docs/commands.md).

## Repository layout

```
engine/          C++ engine: one shared library per module (core, script, network, physics, graphics, audio, app)
plugins/         runtime plugins (graphics-raylib: window, rendering, input; audio-raylib: sound and music)
game/server/     r-type_server entry point
game/client/     r-type_client entry point
game/scripts/    the game itself, in Lua (main.lua is the entry point)
assets/          sprites, sounds, music (credits in assets/CREDITS.md)
tests/           GoogleTest suites, mirroring engine/, game/ and plugins/
docs/            documentation (MkDocs sources)
```

## Documentation

Everything lives in [`docs/`](docs/index.md) as Markdown and is published as a
searchable website with MkDocs on every push to `main`
(GitHub Pages, with the Doxygen API reference under `/api/`).

| Page | Content |
|---|---|
| [Getting started](docs/getting-started.md) | Detailed build, run and troubleshooting |
| [How to play](docs/how-to-play.md) | Controls and rules |
| [Architecture](docs/architecture.md) | Layers, subsystems, data flow, diagrams |
| [Engine modules](docs/engine.md) | What each module does and how to add one |
| [Networking internals](docs/networking.md) | Threads, queues, sessions, failure handling |
| [Protocol](docs/protocol.md) | RFC-style specification of the UDP protocol |
| [Plugins](docs/plugins.md) | Runtime plugin contract |
| [Game](docs/game.md) / [Writing the game](docs/writing-the-game.md) | The Lua game and how to extend it |
| [Project overview](docs/PROJECT.md) | Vision, Lua API reference, roadmap |
| [Comparative study](docs/comparative-study.md) | Technology choices, algorithms, storage, security |
| [Accessibility](docs/accessibility.md) | What we provide for players with disabilities |
| [Commands](docs/commands.md) | Build, test and quality commands |
| [Contributing](docs/contributing.md) | Workflow and conventions |

Preview the website locally:

```sh
pip install "mkdocs==1.6.1" "mkdocs-material==9.7.7"
mkdocs serve                            # http://127.0.0.1:8000
```

## Contributing

Read [docs/contributing.md](docs/contributing.md) and [AGENTS.md](AGENTS.md).
In short: one issue, one branch, one pull request into `dev`, with tests, and
a green CI.

## Authors

- Alban Lembrez
- Amayyas Aouadene
- Frederick Jeffcock
- Louis Bagnol
- Lucas Pujol

## License

Released under the [MIT License](LICENSE). R-Type is a trademark of its
respective owners; the sprites are used for educational purposes only.
The music is by Beam Theory (OGA-BY 3.0); every asset is credited in
[assets/CREDITS.md](assets/CREDITS.md).
