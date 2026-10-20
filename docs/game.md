# Game

The game lives in `game/`. It is R-Type, built on top of the engine. It is
made of two thin C++ executables and the game itself, written in Lua.

```
game/
├── server/main.cpp    → r-type_server
├── client/main.cpp    → r-type_client
└── scripts/           Lua game code, main.lua is the entry point
```

## The contract

The game is a **user** of the engine, exactly like a second game would be.

- It links only `engine::*` targets (`engine::core`, `engine::script`,
  `engine::network`).
- It includes only public engine headers: `#include "engine/<module>/..."`.
  Nothing from `engine/src/`.
- It never links a plugin: the client loads `graphics-raylib` at runtime.
- It never uses a third-party library directly: Lua, Raylib and the OS socket API are
  reached only through the engine.

In the other direction, the engine never knows the game: nothing under
`engine/` names R-Type, its entities or its scripts.

## Executables

```cmake
add_executable(r-type_server server/main.cpp)
target_link_libraries(r-type_server PRIVATE engine::core engine::script engine::network)

add_executable(r-type_client client/main.cpp)
target_link_libraries(r-type_client PRIVATE engine::core engine::script engine::network)
add_dependencies(r-type_client graphics-raylib)
```

| Executable | Usage | Role |
|---|---|---|
| `r-type_server` | `./r-type_server <port> [--max-players <1-4>]` | Authoritative: runs all game logic, sends snapshots. |
| `r-type_client` | `./r-type_client <server-ip> <port>` | Sends inputs, displays what the server replicates. |

Both exit with code `84` on invalid arguments.

In Part 1 each `main` assembles the engine modules explicitly: reading it
shows the whole tick (server) or frame (client). In Part 2 they become thin
entry points over generic `ServerApp` / `ClientApp` classes from the engine.

## Multiplayer demo

`main.lua` gives every connected player a square in the colour of its slot
(`on_player_joined`) and removes it when the player leaves
(`on_player_left`). Arrow keys (or `ZQSD`) move it; every client sees every
square and outlines its own. The server runs the Lua game; the client only
sends inputs and draws the snapshot.

```sh
cmake --build --preset ninja-debug
cd build/ninja-debug/bin
./r-type_server 4242
./r-type_client 127.0.0.1 4242
./r-type_client 127.0.0.1 4242
```

- Move one square: the other window shows the same position.
- Close one client: its square disappears from the other window.

`ServerApp` holds no game rule: it forwards joins, leaves and inputs to the
script runtime and replicates every scripted entity with its declared type,
owner and colour. It is the generic server of Part 2. The client drawing
lives in `game/client/TemporaryNetworkView.*` until the snapshot display
(#23) and the client loop (#24) replace it.

## Scripts

- Every `.lua` file under `game/scripts/` is loaded recursively, except
  `lib/`, which is reached with `require`.
- Each file declares what it contains: `entity "name"` or `system "name"`.
- `main.lua` defines `on_start()`, called once every declaration is loaded.
- Logic runs on the server only; the client loads the same declarations only
  to know how entities look.

The full Lua API is described in [PROJECT.md §10](PROJECT.md#10-lua-scripting-api).
How to structure gameplay code is described in [writing-the-game.md](writing-the-game.md).

## Runtime layout

Executables find everything relative to their own location, never the working
directory:

```
bin/
├── r-type_server
├── r-type_client
├── libengine-core.so
├── libengine-script.so
├── libengine-network.so
├── plugins/graphics-raylib.so
└── scripts/
```

On Linux the RPATH `$ORIGIN` finds the engine libraries; on Windows the DLLs
sit next to the executables.

## Rules

- If a feature would make sense in another game, it belongs in the engine,
  exposed to Lua; otherwise it belongs in the scripts.
- The C++ in `game/` stays minimal: wiring only, no gameplay.
