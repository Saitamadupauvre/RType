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
- It never uses a third-party library directly: Lua, Asio and Raylib are
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
| `r-type_server` | `./r-type_server <port>` | Authoritative: runs all game logic, sends snapshots. |
| `r-type_client` | `./r-type_client <server-ip> <port>` | Sends inputs, displays what the server replicates. |

Both exit with code `84` on invalid arguments.

In Part 1 each `main` assembles the engine modules explicitly: reading it
shows the whole tick (server) or frame (client). In Part 2 they become thin
entry points over generic `ServerApp` / `ClientApp` classes from the engine.

## Scripts

- Every `.lua` file under `game/scripts/` is loaded recursively, except
  `lib/`, which is reached with `require`.
- Each file declares what it contains: `entity "name"` or `system "name"`.
- `main.lua` defines `on_start()`, called once every declaration is loaded.
- Logic runs on the server only; the client loads the same declarations only
  to know how entities look.

The full Lua API is described in [PROJECT.md §10](PROJECT.md#10-lua-scripting-api).

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
