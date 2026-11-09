# Play and debug

## Playing from the editor

**Play** in the editor's menu bar starts the open scene in its own window,
with unsaved changes, and without touching your scene files:

```
engine-runtime --solo --game <your game folder> --scene-file <temp>/engine-editor/played-scene.json
```

- Solo mode runs a server and a client in one process, without any socket.
  It is the same server code as a networked game: what works in solo works
  over the network.
- Everything the game prints goes to the editor's console: `print` from
  scripts, engine messages, Lua errors with their file and line.
- **Pause** freezes the game process; **Play** continues it. **Stop**, or
  closing the game window, ends it. The console shows the exit code.
- While the game runs, the editor refuses edits and scene changes, so what
  you play is what you see.

Scripts are read when the game starts: save a `.lua` file and press Play
again to try it. There is no reload while playing yet.

## When something goes wrong

Every Lua call is protected. An error never closes the game or the editor:

| Error | What happens |
|---|---|
| Syntax error, duplicate name, bad declaration | Logged at startup with the file; that file's declarations are not registered |
| Error in an entity callback | Logged with file and line (`enemy.lua:14: attempt to index a nil value`); that entity is disabled, the game goes on |
| Error in `main.lua` callbacks (`on_player_joined`...) | Logged; the game goes on |
| Wrong value for a native field (`self.hitbox = { w = -1, h = 2 }`) | Lua error naming the field; the entity keeps its old value |
| Undeclared sound, texture or event | Lua error at the call |
| Bad scene file or instance | The file or the instance is skipped and logged; the rest loads |
| Missing asset | Drawn as a placeholder, logged once |
| A name written after a table-form `entity "x" { ... }`, or a misspelled callback (`on_collision`) | Warning at load, also in the editor's Problems tab: the engine would ignore it otherwise |
| A call to a planned function such as `after` | The nil-call error says it is planned but not implemented yet |

Common mistakes:

- `self.method()` instead of `self:method()`.
- A field read before it is set: `vx` and `vy` read `nil` until the entity
  moves, so write `(self.vx or 0)`.
- A server-only function (`spawn`, `play`, `load_scene`) at the top level of a
  file, or in a `client` section.

To check scripts without opening a window, run the game as a dedicated server:
load errors are printed at once.

```sh
engine-runtime --game my-game --server 4242
```

## Metrics overlay

`F3` in the game window shows a panel over the game:

| Line | What it shows |
|---|---|
| `FPS`, `frame`, `max` | Frames per second, average and worst frame time over the last 2 s |
| `Lua`, `physics`, `draw` | Time per frame spent in client scripts, local physics and drawing |
| `Server tick` | In `host` and `solo` modes: tick, Lua and physics time of the server |
| `Entities` | Entities on screen; in `host` and `solo` modes, also on the server |
| `CPU`, `memory` | CPU use of the process, in % of one core, and its memory |
| `Ping`, `lost`, `snapshots` | Round-trip time, packets lost, world updates received per second |
| `In`, `out` | Network traffic per second |

Below the text, a graph shows the frame time of the last 2 s: green is faster
than 60 FPS, yellow faster than 30 FPS, red slower.

## Developer console

`` ` `` (the key left of `1` on QWERTY) or `F1` opens a console over the game.
While it is open, the game receives no key.

| Line | What it does |
|---|---|
| `help` | Lists the commands |
| `clear` | Empties the console |
| `cvar` or `cvar list [text]` | Lists the console variables, or those whose name contains `text` |
| `cvar <name>` | Shows a variable: value, kind, default, range and description |
| `cvar <name> <value>` | Changes it; booleans take `on`/`off`, `true`/`false`, `yes`/`no` or `1`/`0` |
| `cvar reset <name>` | Restores its default |
| `/server <lua>` | Runs Lua on the server, in `solo` and `host` modes only |
| anything else | Runs Lua in the client: `1 + 1` prints `2`, `print(x)` prints `x` |

| Key | Action |
|---|---|
| `Enter` | Run the line |
| `Up`, `Down` | Walk the lines run before |
| `Page Up`, `Page Down` | Scroll |
| `Escape`, `` ` ``, `F1` | Close |

Every log message also shows in the console, warnings and errors in red. An
error in a line is printed and the game goes on.

Try on the server side, in solo mode:

```
/server spawn("coin", { x = 960, y = 540 })
/server for _, p in ipairs(players()) do print(p.id, p.slot) end
```

### Console variables

| Variable | Side | Effect |
|---|---|---|
| `debug.metrics` | client | Same as `F3` |
| `debug.hitboxes` | client | Outlines the colliders of the client's local entities |
| `audio.music_volume`, `audio.effects_volume` | client | Volume, from `0` to `1` |
| `audio.muted` | client | Mutes every sound |
| `sv.tick_rate` | server | Server ticks per second, 1 to 240 (60 by default): `/server cvar.set("sv.tick_rate", 30)` |

Games add their own with `cvar.register { name = "game.lives", default = 3, min = 0, max = 9 }`
and read them with `cvar.get("game.lives")`; see the
[Lua API](../reference/lua-api.md#developer-console-and-console-variables).
