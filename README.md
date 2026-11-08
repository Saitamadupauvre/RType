# Engine

A 2D multiplayer game engine written in C++20, with an **editor** to make
games and a **runtime** to play and ship them. A game is a folder of Lua
scripts, JSON scenes and assets: you lay out its levels in the editor, press
Play to try it, and export it as a standalone folder. Every game is networked
from the start, with up to four players on an authoritative UDP server.

The repository ships one example game, **R-Type**, a cooperative shoot'em up
in [`examples/r-type/`](examples/r-type/) made only of Lua, scenes and assets.

Epitech *Advanced C++* project (B-CPP-500).

- **Full documentation:** [`docs/`](docs/index.md), published online with
  MkDocs (see [Documentation](#documentation)).
- **Make your first game:** [docs/guide/first-game.md](docs/guide/first-game.md).

## Contents

- [Features](#features)
- [Supported platforms](#supported-platforms)
- [Requirements](#requirements)
- [Build](#build)
- [Open the editor](#open-the-editor)
- [Run a game](#run-a-game)
- [Install](#install)
- [Tests and checks](#tests-and-checks)
- [Repository layout](#repository-layout)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [Authors](#authors)
- [License](#license)

## Features

**Editor**

- Opens a game folder and lists its prefabs (the entity types its scripts
  declare), its scenes and the instances of the open scene.
- Viewport drawn like the game: place prefabs by drag and drop, select, move,
  scale and rotate with grid snapping, pan and zoom, hitbox and name overlays.
- Inspector built from the engine's field registry and the scripts' fields:
  every placed entity can override any field of its prefab, with reset.
- Undo and redo, unsaved-change tracking, stable scene files that diff well.
- **Play** runs the open scene in the real game, unsaved changes included,
  with the game's output and Lua errors in the editor's console.
- **Export** writes a self-contained folder: the runtime renamed after the
  game, the engine libraries, the plugins and the game's files.
- Menu bar, command palette (`Ctrl+Shift+P`), personal `settings.json`.

**Runtime and engine**

- `engine-runtime` runs any game folder: solo (no network), host (play and let
  others join), join, or dedicated server.
- Authoritative multithreaded server (receive, game and send threads) at a
  fixed 60 ticks per second; custom binary UDP protocol, validated before every
  read and documented RFC-style.
- Gameplay in sandboxed Lua 5.4: entities, collisions, input, sound, music,
  game events, client HUD and local effects, cameras, pathfinding, scenes,
  console variables.
- A crashed or disconnected client is dropped after 5 seconds and the others
  are notified; malformed packets are dropped and logged; a Lua error disables
  only the faulty entity.
- Engine split into shared libraries (`core`, `script`, `network`, `physics`,
  `navigation`, `graphics`, `audio`, `app`, `editor`), with the Raylib
  graphics and audio backends loaded at runtime as plugins.
- In-game metrics overlay (`F3`) and developer console running Lua on the
  client or the server.

## Supported platforms

| Platform | Editor | Runtime | Tested in CI |
|---|---|---|---|
| Linux (GCC, Clang) | Yes | Yes | Yes |
| macOS (Apple Clang) | Yes | Yes | Yes |
| Windows | No | No | No |

Windows support is postponed: the network module only has a POSIX socket
implementation and Play uses POSIX processes. See the
[comparative study](docs/comparative-study.md#cross-platform-support).

## Requirements

| Tool | Version | Notes |
|---|---|---|
| C++ compiler | Recent GCC, Clang or Apple Clang | C++20; CI uses the compilers of `ubuntu-latest` and `macos-latest` |
| CMake | 3.25+ | |
| Ninja | any | Optional; used by the `ninja-*` presets |
| Git | any | Needed by vcpkg |
| [vcpkg](https://github.com/microsoft/vcpkg) | any recent | Builds every third-party library |

Every third-party library (Raylib, Lua, sol2, Clay, nlohmann-json, GoogleTest,
and [`clap`](https://github.com/lucaspujol/clap), our command-line parser
served from the overlay port in `ports/`) comes from [`vcpkg.json`](vcpkg.json)
and is built automatically the first time you configure. Nothing has to be
installed system-wide except the compiler toolchain and, on Linux, the
X11/OpenGL headers Raylib needs.

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
git clone https://github.com/Saitamadupauvre/RType.git engine
cd engine
cmake --preset release            # first run builds the dependencies: a few minutes
cmake --build --preset release
```

`build/release/bin/` then holds `editor`, `engine-runtime`, the engine
libraries and the `plugins/` folder. This is the only compilation: games are
never compiled, the runtime reads their scripts and assets when it starts.

| Preset | Generator | Use |
|---|---|---|
| `release` / `debug` | Default generator | Using / debugging |
| `ninja-release` / `ninja-debug` | Ninja | Faster builds |
| `asan`, `tsan`, `tidy`, `ci` | Default generator | Checks, see [Tests and checks](#tests-and-checks) |

The output folder is always `build/<preset>/`.

## Open the editor

```sh
./build/release/bin/editor examples/r-type
```

The editor opens the R-Type example on its start scene. Drag prefabs from the
sidebar into the viewport, select one to edit its fields in the inspector,
save with `Ctrl+S`, and press **Play** to run the scene in a game window.

| Argument | Meaning |
|---|---|
| `game` | Game folder to edit; it must contain a `game.json` |
| `--runtime <program>` | Program Play starts; defaults to `engine-runtime` next to the editor |
| `--log-level <level>` | Developer log on stderr: `trace`, `debug`, `info` (default), `warn`, `error`, `off` |

To make your own game, follow [Your first game](docs/guide/first-game.md).

## Run a game

```sh
./build/release/bin/engine-runtime --game examples/r-type --solo            # play alone, no socket
./build/release/bin/engine-runtime --game examples/r-type --host 4242       # host and play; others join
./build/release/bin/engine-runtime --game examples/r-type 127.0.0.1 4242    # join a server
./build/release/bin/engine-runtime --game examples/r-type --server 4242     # dedicated server, no window
```

| Option | Meaning |
|---|---|
| `server-ip` `port` | Join that server |
| `--host <port>` | Host a game on this UDP port and play on it |
| `--server <port>` | Dedicated server on this UDP port |
| `--solo` | Play alone, without network |
| `--game <folder>` | Game folder; defaults to the program's own folder (exported games) |
| `--scene-file <file>` | Start from this scene instead of the game's start scene |
| `-p`, `--max-players <1..4>` | Maximum players when hosting or serving, default `4` |
| `--music-volume`, `--effects-volume <0..100>` | Volumes, default `100` |
| `--mute` | Start muted |
| `-h`, `--help` | Print the usage and exit with code `0` |

Invalid arguments, a port already in use or a missing graphics plugin exit
with code `84`. A missing audio device only disables sound. Over a local
network, open the UDP port in the host's firewall and join with its IPv4
address. Everything is in [Run and export](docs/guide/run-and-export.md).

In R-Type, move with the arrow keys or `ZQSD`, fire with `Space`, mute with
`M`; see the [R-Type example](docs/examples/r-type.md).

## Install

```sh
cmake --install build/release --prefix dist
```

`dist/bin/` contains the editor, the runtime, the engine libraries and
`plugins/`; `dist/examples/` contains the example games. Executables find
their libraries and plugins relative to their own location, so the folder can
be moved as a whole. Games themselves are shipped with the editor's
**Export**.

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

`scripts/fred.sh all` runs the format check, the debug build, the tests and
clang-tidy in one go; `scripts/fred.sh editor` and `scripts/fred.sh play --solo`
build and open the example. Every command is described in
[docs/commands.md](docs/commands.md).

## Repository layout

```
engine/          C++ engine: one shared library per module (core, script, network, physics, navigation, graphics, audio, app, editor)
plugins/         runtime plugins (graphics-raylib: window, rendering, input; audio-raylib: sound and music)
editor/          the editor app (Clay layout, raylib only in editor/src/raylib/)
runtime/         engine-runtime: runs any game folder in every mode
examples/r-type/ example game: game.json, scripts/, scenes/, assets/ (credits in assets/CREDITS.md)
tests/           GoogleTest suites, mirroring engine/, plugins/, editor/ and runtime/
docs/            documentation (MkDocs sources)
```

## Documentation

Everything lives in [`docs/`](docs/index.md) as Markdown and is published as a
searchable website with MkDocs on every push to `main` (GitHub Pages, with the
Doxygen API reference under `/api/`).

| Page | Content |
|---|---|
| [Getting started](docs/getting-started.md) | Build, open the example, troubleshooting |
| [Making games](docs/guide/index.md) | The editor, your first game, game folders and scenes, scripting, play and debug, run and export |
| [Lua API reference](docs/reference/lua-api.md) | Every function scripts can call |
| [R-Type example](docs/examples/r-type.md) | How to play it, how it is built, how to extend it |
| [Architecture](docs/architecture.md) | Layers, subsystems, data flow, diagrams |
| [Engine modules](docs/engine.md) | What each module does and how to add one |
| [Networking internals](docs/networking.md) / [Protocol](docs/protocol.md) | Threads, sessions, failure handling; RFC-style UDP protocol |
| [Plugins](docs/plugins.md) | Runtime plugin contract |
| [Editor backend](docs/editor.md) / [Editor internals](docs/editor-app.md) | Editing model; editor app code |
| [Project overview](docs/PROJECT.md) | Vision, architecture decisions, roadmap |
| [Comparative study](docs/comparative-study.md) | Technology choices, algorithms, storage, security |
| [Accessibility](docs/accessibility.md) | What we provide for people with disabilities |
| [Commands](docs/commands.md) / [Contributing](docs/contributing.md) | Build, test, quality commands; workflow and conventions |

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
respective owners; the example's sprites are used for educational purposes
only. The example's music is by Beam Theory (OGA-BY 3.0); every asset is
credited in [examples/r-type/assets/CREDITS.md](examples/r-type/assets/CREDITS.md).
