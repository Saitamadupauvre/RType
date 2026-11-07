# Editor app

`editor` is the window in front of the [editor backend](editor.md): it shows
the open game folder and turns clicks and keys into **actions**. All the
editing logic lives in `engine-editor`; the app only displays it and asks for
changes.

```sh
editor game          # edit ./game and open its start scene
editor --help
```

The argument is required and must be a folder that contains a `game.json`
(see [Game folder and scenes](game-folder.md)). Opening the editor without a
project, and creating one, come with the launcher (#158).

## Window

| Area | Shows | Does |
|---|---|---|
| Menu bar | Open scene, `•` when unsaved | Play, Stop |
| Toolbar | Tools, snap, grid size, overlays | Switch tool, toggle snap and overlays, Undo, Redo |
| Sidebar | Prefabs, scenes (start scene marked), instances | Open a scene, select an instance |
| Viewport | The scene (#155) | |
| Inspector | Name, type, position and overrides of the selection | Field editing comes with #156 |
| Bottom panel | Console, Problems | Switch tab |
| Status bar | Mode, instance count, game folder, scene | |

| Shortcut | Command |
|---|---|
| Ctrl+S | Save scene |
| Ctrl+Z / Ctrl+Y | Undo / Redo |
| Delete | Delete the selection |
| V / G / H | Select, move, pan tool |
| F11 | Clay debug view |

On macOS, Cmd replaces Ctrl.

## One-way flow

```
input ──▶ build_frame(ui, editor) ──▶ render ──▶ editor.handle(action) for each action
              reads state, emits actions                 changes session and state
```

Each frame, the UI **reads** `Editor::state()` and `Editor::session()` and
**emits** actions; it never changes anything itself. Once the frame is drawn,
the loop hands every action to `Editor::handle()`. Results come back as state:
a refused action adds an error line to the console instead of calling back the
UI.

This keeps the UI testable without a window (state and input in, actions out)
and means a frame never draws half-changed data.

## Actions

```cpp
using Action = std::variant<Edit, Command>;
```

- **`Edit`** changes the open scene: place, move, remove, rename, set field,
  reset field, reset all. Edits are undoable, mark the scene unsaved, and are
  refused outside edit mode.
- **`Command`** is everything else: selection, history, save, scene files,
  view settings, tools, tabs, play, pause, stop. Each command declares
  `while_running`; those that change the project (undo, redo, discard, scene
  commands) declare `false` and are refused while the game plays or is paused.

One action per user intent: a drag emits one `Move` when it ends, a text field
one `SetField` when it commits. Filters are the exception: the prefab filter
emits `SetPrefabFilter` on every change so the list follows the typing.

`Editor::handle()` visits the variants with `Overloaded` and no catch-all
branch, so an action without a handler does not compile.

### Adding an action

1. Add the struct to `editor/src/action/Action.hpp` and to its variant. A
   command also states `while_running`.
2. Handle it in `ApplyEdit.cpp` or `ApplyCommand.cpp`; the build fails until
   you do.
3. Emit it from the UI, and add it to the command table if it deserves a name
   or a shortcut.
4. Test the handler in `tests/editor/action/` and the emitting UI in
   `tests/editor/ui/`.

## Command table

`ui/CommandTable.cpp` lists the commands users can name: a label, an optional
shortcut, and a `make` function that returns the action, or `std::nullopt`
when the command is disabled (nothing to undo, no selection...). Keyboard
shortcuts read it today; menus and the command palette (#185) will read the
same table, so they never disagree.

## Writing UI

- Panels live in `ui/sections/`, one file each; reusable pieces in
  `ui/widgets/` (`button`, `toggle`, `list_row`...).
- A section takes the frame's `Ui` and the `const Editor&`. `Ui` gives the
  theme, the input, the widget state, the platform, `clicked(id)` and
  `emit(action)`.
- Clickable elements need a stable Clay id: `CLAY_ID("PlayButton")`, or
  `CLAY_IDI("SceneRow", index)` in lists. Hover and clicks use the previous
  frame's layout, as usual in immediate mode.
- Clay keeps pointers to text until the frame is rendered. Text from the state
  or the session can be passed with `view_of()`; formatted text must go
  through `ui.keep(std::format(...))`.
- Sizes come from `ui/Theme.hpp` through `theme.px()`, so the whole UI follows
  the window's DPI scale.

## Widgets

Input widgets follow the one-way flow: they take the current value, never a
reference, and return what happened this frame. The caller emits the action.

```cpp
const TextResult name = text_field(ui, CLAY_ID("NameField"), instance.name);
if (name.event == FieldEvent::committed) {
    ui.emit(edit::Rename{.id = instance.id, .name = name.value});
}
```

| Widget | Returns |
|---|---|
| `text_field(ui, id, value, options)` | `TextResult`: the draft (`changed`), the text (`committed`) or the restored value (`cancelled`) |
| `number_field(ui, id, float or int, options)` | `FieldResult<float or int>`: the parsed value on a valid commit, `cancelled` otherwise |
| `checkbox(ui, id, value, label)` | `true` when clicked; the caller emits `!value` |
| `dropdown(ui, id, selected, options)` | the index picked this frame, if any |

- **Text field**: a click focuses it on the next frame. While focused it edits
  a draft with a UTF-8 aware caret and selection. Enter, Tab or a click
  outside commits; Esc restores the value. Tab and Shift+Tab move through the
  fields drawn this frame. A read-only field never takes focus.

  | Input | Effect |
  |---|---|
  | Arrows, Home, End | move the caret; with Shift, extend the selection |
  | Ctrl+Left / Ctrl+Right | move by word; with Shift, select by word |
  | Backspace / Delete | erase a character; with Ctrl, a word |
  | Ctrl+A / C / X / V | select all, copy, cut, paste |
  | Click, Shift+click, drag | place the caret, extend, select |
  | Double / triple click | select the word, select everything |

  Letter shortcuts follow the keyboard layout: Ctrl+A is the key printed A,
  on AZERTY as on QWERTY.
- **Number field**: a text field on the formatted value. A commit that does not
  parse as a finite number is reverted; a valid one is clamped to `min`/`max`
  and to the type's range, and rounded for `int`.
- **Dropdown**: the list floats over the layout, unclipped by scroll
  containers. Up/Down move the highlight, Enter picks, Esc or a click outside
  closes.

### Widget state

`WidgetState` is the only state widgets keep between frames: the focused
field, its draft and original value, the open popup and its highlight. The
frame loop owns it and lends it to each frame's `Ui`; `build_frame` calls
`end_frame()` once the layout is done.

- Focus changes requested during a frame apply on the next one, so the field
  losing focus always commits first.
- A focused field that is not drawn in a frame loses focus.
- `wants_keyboard()` is true while a field is focused: shortcuts are ignored
  then, and the viewport must ignore keys too.

### Platform

`Platform` holds the services the widgets need from the window, as functions:
text measuring and clipboard read/write. The raylib loop fills it; tests pass
fakes. `UiInput` carries the typed UTF-8 text and the pressed and repeated
keys.

## Code layout

```
editor/
  assets/fonts/   JetBrains Mono, copied next to the executable
  src/action/     Action, Editor, EditorState: no Clay, no raylib
  src/cli/        command line (clap)
  src/clay/       Clay context
  src/text/       TextEdit: UTF-8 caret, selection and editing, no UI
  src/ui/         layout, sections, widgets, widget state, command table
  src/render/     geometry and scissor helpers for the renderer
  src/raylib/     window, input, Clay renderer, font cache: the only raylib code
```

Everything outside `src/raylib/` builds into `editor-core` and is tested
headless in `tests/editor/` (`editor_add_test`).
