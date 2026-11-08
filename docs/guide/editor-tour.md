# The editor

The editor opens one game folder, shows its scenes, and turns clicks into
changes to the open scene. It never edits scripts: behaviour lives in Lua,
layout lives in scenes.

```sh
editor examples/r-type                      # open a game folder and its start scene
editor examples/r-type --log-level debug    # also log every action on stderr
editor --help
```

| Argument | Meaning |
|---|---|
| `game` | Game folder to edit. Required; it must contain a `game.json` ([Game folders](game-folder.md)). |
| `--runtime <program>` | Program Play starts. Defaults to `engine-runtime` next to the editor. |
| `--log-level <level>` | `trace`, `debug`, `info` (default), `warn`, `error` or `off`. See [Developer logs](#developer-logs). |

The editor opens the `start_scene` of `game.json`. Without one, it opens no
scene: click one in the sidebar. Opening the editor without a folder, and
creating a new game from the editor, are not there yet: create the folder by
hand as in [Your first game](first-game.md).

## Window

```
┌───────────────────────────────────────────────────────────────────────┐
│ File  Edit  View  Scene  Help            stage1 •          Play Export│  menu bar
│ [Select][Move][Pan]  Snap  Grid 16  Hitboxes  Names     Undo  Redo    │  toolbar
├────────────┬───────────────────────────────────────────┬──────────────┤
│ PREFABS    │                                           │ INSPECTOR    │
│  ship      │                                           │  name        │
│  enemy     │               viewport                    │  type ▾      │
│ SCENES     │                                           │  position    │
│  stage1 ★  │                                           │  components  │
│ INSTANCES  │                                           │  script      │
│  space     │                                           │              │
├────────────┴───────────────────────────────────────────┴──────────────┤
│ CONSOLE | PROBLEMS                                                     │  bottom panel
├───────────────────────────────────────────────────────────────────────┤
│ EDIT · 12 instances · enemy_1 · snap 16 · unsaved · game · stage1      │  status bar
└───────────────────────────────────────────────────────────────────────┘
```

| Area | Shows | Does |
|---|---|---|
| Menu bar | File, Edit, View, Scene and Help menus; the open scene, with `•` when it has unsaved changes | Every command, Play, Stop, Export |
| Toolbar | Tools, snap, grid size, overlays | Switch tool, toggle snap and overlays, Undo, Redo |
| Play band | While the game runs or is paused: the mode and what is blocked | |
| Sidebar | **Prefabs** (every entity type the scripts declare, with a filter), **scenes** (the start scene is marked), **instances** of the open scene | Place a prefab, open a scene, select an instance |
| Viewport | The scene drawn like the game, the grid, the camera frame, the selection | Select, move, scale, rotate, pan, zoom |
| Inspector | The selected instance: name, prefab, script, fields by component | Rename, change type, edit and reset fields, open the script |
| Bottom panel | **Console** (timestamped messages, game output during Play) and **Problems** | Switch tab |
| Status bar | Mode, instance count, selection, snap, unsaved changes, game folder, scene | |

## Words

| Word | Meaning |
|---|---|
| **Prefab** | An entity type declared in Lua with `entity "name"`. Its fields give the default of every instance. |
| **Instance** | One entity placed in a scene: a prefab, a position, an optional name, and the fields it overrides. |
| **Override** | A field whose value differs from the prefab, stored in the scene. Changing the prefab in Lua changes every instance that does not override that field. |
| **Scene** | A JSON file of instances in `scenes/`. The runtime spawns it when the game starts or when a script calls `load_scene`. |

## Viewport

Instances are drawn like the game draws them: the position is the top-left
corner, a sprite takes its texture size times its scale, an entity with a
color and no sprite is a 64 px square of that color, and `z_index` orders the
drawing. Entities the game does not draw (no sprite, no color) show as a crossed
outline so they can still be selected. Textures load from the game's
`assets/`; a missing one is drawn as the tinted square.

| Input | Effect |
|---|---|
| Click an instance | Select it (the topmost one when they overlap) |
| Click empty space | Clear the selection |
| Drag with the Move tool (`G`) | Move the instance; one undoable step when you release |
| `Alt` while dragging | Drop without snapping |
| Middle drag, or drag with the Pan tool (`H`) | Pan |
| Wheel | Zoom around the cursor, 0.1× to 10× |
| Drag a prefab from PREFABS into the viewport | Place an instance under the cursor, snapped |
| Click a prefab, then click in the viewport | Same; `Esc` or a click elsewhere cancels |
| Drag the square handle of the selection | Scale it (only if the prefab has a `scale`) |
| Drag the round handle of the selection | Rotate it, by 15° steps with snap on (only if the prefab has a `rotation`) |
| `Delete` | Delete the selection |

| Overlay | Shows |
|---|---|
| Camera frame (always) | What the game shows at start: the view of the first instance with a camera (`zoom`, `follow` or `bounds`), or the 1920 × 1080 screen from the origin |
| Names (toolbar) | Each instance's name, or its type |
| Hitboxes (toolbar) | Rectangle and circle colliders, where the physics puts them |

With snap on, a dragged instance's top-left corner lands on the grid. The
world is 1920 × 1080 virtual units with the origin at the top-left and Y
pointing down, the same units scripts use.

## Inspector

The inspector shows the selected instance: a header with its name and
`instance of <prefab> · <script>`, then its fields.

- **Name** is a label for you; scripts never see it, and it need not be unique.
- **Position** moves the instance.
- **Type** switches the instance to another prefab. The name, the position and
  the overrides the new prefab accepts are kept; the console lists the ones
  dropped. Undo brings everything back.
- **Script** names the file declaring the prefab; **Open** opens it in your
  system's program for `.lua` files.
- **Components** lists the engine fields of the prefab (sprite, color, hitbox,
  physics body...), one band per component.
- **Script** lists the [script fields](game-folder.md#script-fields) the prefab
  declares, such as `health = 3`.

Each field shows its override, or the prefab value when there is none. An
overridden field has an accent label and a **Reset** button; **Reset all to
prefab** clears every override of the instance.

| Field kind | Edited with |
|---|---|
| float, int | Number field |
| bool | Checkbox |
| string | Text field |
| vec2 | Two number fields, x and y |
| color | Swatch and a hex field: `#RRGGBB` or `#RRGGBBAA` |
| asset | Path relative to `assets/` |
| list | One row per item, `x` to remove it, `+ Add` to append |

A value is committed with `Enter`, `Tab` or a click elsewhere; `Esc` restores
it. A value that does not parse is refused with a message in the console.

## Scenes and saving

- Click a scene in SCENES to open it. A scene with unsaved changes must be
  saved (`Ctrl+S`) or discarded (File > Discard changes) first.
- Every edit can be undone (`Ctrl+Z`) and redone (`Ctrl+Y`), up to 100 steps.
  Undoing back to the saved state clears the unsaved mark.
- Saving writes only the open scene file. Overrides are sorted by key, so two
  saves of the same scene give the same file and diffs stay small.
- **Problems** lists what the editor could not build for an instance of the
  open scene: an override of the wrong kind, a value the component refuses.
- Instances whose type no script declares (a typo, a deleted script) are
  reported when the scene opens and kept in the file exactly as they were.

Creating, duplicating, renaming and deleting scenes, and changing the start
scene, exist in the editor's model but have no buttons yet. Until they do,
create scene files by hand (see [Your first game](first-game.md#3-an-empty-scene))
and set `start_scene` in `game.json`.

## Play

**Play** runs the open scene in the real game, in its own window:

1. The scene, unsaved changes included, is written to a temporary file. Your
   scene files are never touched by playing.
2. The runtime starts as `engine-runtime --solo --game <folder> --scene-file <temp file>`:
   a server and a client in one process, starting from that scene instead of
   the `start_scene`.
3. Everything the game prints, Lua errors included, goes to the console. A
   Lua error never closes the editor.

| Command | Effect |
|---|---|
| Play | Start the open scene; on a paused game, continue it |
| Pause | Freeze the game process |
| Stop | Ask the game to quit, kill it after one second |

While the game runs, edits and scene changes are refused ("Stop the game to
change the project."). Closing the game window ends Play too. More in
[Play and debug](play-and-debug.md).

## Export

**Export** (or File > Export game...) writes a folder that runs the game
without the editor. See [Run and export](run-and-export.md#exporting-from-the-editor).

## Menus, shortcuts and command palette

| Menu | Commands |
|---|---|
| File | Save scene, Discard changes, Open script, Open settings, Reload settings, Export game... |
| Edit | Undo, Redo, Delete selection |
| View | Select tool, Move tool, Pan tool, Snap to grid, Show hitboxes, Show names |
| Scene | Play, Pause, Stop |
| Help | The viewport controls that have no menu entry |

| Shortcut | Command |
|---|---|
| `Ctrl+S` | Save scene |
| `Ctrl+Z` / `Ctrl+Y` | Undo / Redo |
| `Delete` | Delete the selection |
| `V` / `G` / `H` | Select / Move / Pan tool |
| `Ctrl+Shift+P` | Command palette |
| `F11` | Layout debug view |

On macOS, `Cmd` replaces `Ctrl`. Letter shortcuts follow the keyboard layout:
`Ctrl+Z` is the key printed Z, on AZERTY as on QWERTY. Shortcuts are ignored
while a text field has the keyboard.

The **command palette** (`Ctrl+Shift+P`) lists every command with a filter.
The filter is fuzzy: `sv` finds "Save scene" and `stg` finds "Snap to grid".
`Up` and `Down` move, `Enter` runs, `Esc` closes. Disabled commands are greyed
out.

## Settings

Personal preferences live in a `settings.json` read at startup. File > Open
settings opens it (creating it with `{}` if needed); File > Reload settings
applies it at once.

| System | File |
|---|---|
| Linux | `$XDG_CONFIG_HOME/engine-editor/settings.json`, or `~/.config/engine-editor/settings.json` |
| macOS | `~/Library/Application Support/engine-editor/settings.json` |

```json
{
    "ui.scale": 1.25,
    "ui.scroll_speed": 1,
    "view.snap": true,
    "view.grid_size": 16,
    "view.show_hitboxes": false,
    "view.show_names": true
}
```

| Key | Type | Range | Default | Effect |
|---|---|---|---|---|
| `ui.scale` | number | 0.5 to 3 | 1 | Multiplies the screen's DPI scale: every panel, font and row |
| `ui.scroll_speed` | number | 0.25 to 4 | 1 | Multiplies the 40 px one wheel notch scrolls |
| `view.snap` | bool | | true | Snap to the grid when the editor starts |
| `view.grid_size` | number | 1 to 512 | 16 | Grid size, in pixels, when the editor starts |
| `view.show_hitboxes` | bool | | false | Show hitboxes when the editor starts |
| `view.show_names` | bool | | true | Show names when the editor starts |

A bad file never stops the editor: invalid JSON gives one error in the console
and the defaults; an unknown key, a wrong type or a value out of range gives a
warning and that key keeps its default. Settings belong to you, not to the
game: a game's window, scenes and controls live in its folder.

## Developer logs

The console is for you as a game maker. What the editor itself does goes to
stderr, at the level of `--log-level`:

| Level | Logged |
|---|---|
| `info` | The game folder and start scene at startup; every refused action with its reason |
| `debug` | Every action with its fields, each font loaded, raylib's own messages |
| `trace` | Every key pressed and the shortcut it ran or why it was skipped |

How the editor is built is described in [Editor internals](../editor-app.md).
