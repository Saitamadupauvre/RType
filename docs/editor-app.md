# Editor internals

`editor` is the window in front of the [editor backend](editor.md): it shows
the open game folder and turns clicks and keys into **actions**. All the
editing logic lives in `engine-editor`; the app only displays it and asks for
changes. What the editor does for its users is described in
[The editor](guide/editor-tour.md); this page is about its code.

## Where each feature lives

| Feature | Code | Notes |
|---|---|---|
| Command line | `cli/Cli` | clap; `--runtime` defaults to `engine-runtime` next to the editor |
| Settings | `settings/Settings` | `settings_path()` per system, `load_settings()` never throws; the file is written only by Open settings when missing |
| Developer logs | `action/ActionText` | Names each action and its fields for the `debug` log level |
| Viewport | `viewport/` (`ViewCamera`, `Snap`, `Handles`, `SceneItems`), `raylib/ViewportPainter` | The camera, the drag and the pan live in `WidgetState::viewport()`; the renderer paints the frame's `ViewportFrame`, passed through the Clay element's custom data |
| Inspector | `action/InspectorModel`, `ui/sections/Inspector*` | `Editor` caches what it shows in `EditorState::inspector` (`inspect_selection`) and refreshes it after every action, so the UI never queries the session |
| Play | `play/PlaySession`, `play/ProcessLauncher` | `PlaySession` owns the game process and the played scene file; `ProcessLauncher` is the POSIX launcher (`posix_spawn` with the output piped to the console, `SIGSTOP`, `SIGTERM`), tests give a fake `GameLauncher` |
| Export | `exporter/GameExport` | `plan_export()` only reads the disk, `run_export()` writes it: the copies, the chosen `modes` into the exported `game.json` (`engine::script::write_modes`), the `<name>-server` launcher and `README.txt`; the runtime folder is attached with `Editor::attach_runtime()` |
| Command palette | `ui/sections/CommandPalette`, `text/FuzzyMatch` | Reads the command table |
| New game, new prefab | `project/ProjectFiles` | `create_project()` (for `--new`) and `create_prefab()` write files without a display; `main` creates the game before opening it |
| Name dialog | `ui/sections/NameDialog` | `EditorState::name_dialog` holds what it creates and the name typed; `CreatePrefab` and `CreateScene` close it only when they succeed |
| Clickable console lines | `text/ScriptLocation` | `Editor::log()` stores in `ConsoleLine::script` the existing script a line names (`player.lua:12:`), so drawing never touches the disk |
| Assets tab | `assets/AssetList`, `ui/sections/BottomPanel`, `raylib/ViewportPainter::paint_thumbnail` | `EditorState::assets` caches `list_assets()` with the prefabs and scenes; a drag lives in `ViewportState::asset_drag` and ends in the viewport as one `SetField`; thumbnails are Clay image elements pointing at the row's `AssetEntry`, drawn from the viewport's texture cache |
| Refusals, drag chip | `ui/sections/Overlays` | `Editor` counts refusals in `EditorState::refusal`; the toast shows a new count for 3 s (`WidgetState::toast()`); the chip follows the pointer during an asset or prefab drag |
| Console cap | `action/Editor` | `Editor::log()` keeps the last `max_console_lines` (500) lines: a flood of game output once overflowed Clay's element count |
| Bottom panel size | `ui/sections/BottomPanel` | The chosen height, unscaled, lives in `WidgetState::bottom_panel()`; `bottom_panel_height()` keeps it between half and twice the design height and leaves the workspace its minimum; the grip asks for the resize cursor with `Ui::request_cursor()`, which `EditorWindow` applies |
| UI zoom | `ui/CommandTable`, `command::SetUiScale` | Steps `EditorState::ui_scale` by 0.1 within `min_ui_scale` and `max_ui_scale`; `Shortcut::any_shift` lets `Ctrl+=` and `Ctrl++` share an entry, and `=`, `+`, `-` are read from the layout like letters |

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

`ui/CommandTable.cpp` lists the commands users can name: a label, the menu it
belongs to, an optional shortcut, a `make` function that returns the action, or
`std::nullopt` when the command is disabled (nothing to undo, no selection...),
and for toggles a `checked` function. Keyboard shortcuts and the menu bar read
the same table, as does the command palette, so the three never disagree.

### Menus

- Clicking a title opens its menu; clicking it again, pressing Esc or clicking
  outside closes it. While a menu is open, hovering another title opens that
  one instead.
- Each entry shows its shortcut (`Ctrl+S`, `Cmd+S` on macOS). A disabled entry
  is greyed out and does nothing. A toggle that is on shows a check mark.
- File: save, discard changes, open the selection's script. Edit: undo, redo,
  delete. View: tools, snap and overlays. Scene: play, pause, stop.
- Help lists the viewport controls that have no entry: pan, zoom, placing,
  moving without snap.
- To add an entry, add a `CommandEntry` with its `menu`; nothing else changes.

### Command palette

- `Ctrl+Shift+P` (`Cmd+Shift+P` on macOS) opens a list of every command over
  the window, with a field to filter it. Help lists the shortcut too.
- The filter is fuzzy: the typed letters must appear in order, ignoring case,
  and matches at word starts or in a row rank first, so `sv` finds "Save
  scene" and `stg` finds "Snap to grid". `text/FuzzyMatch` scores and ranks.
- Each row shows the command, its menu and its shortcut. Disabled commands are
  greyed out and never run.
- Up and Down move the highlight, Enter or a click runs the command and closes
  the palette, Esc or a click outside closes it. Tab keeps the keyboard in the
  palette.
- While it is open, its field holds the keyboard, so shortcuts are ignored.
  It does not open over the export dialog.
- A new `CommandEntry` shows up in the palette with nothing else to change.

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
| `number_field(ui, id, float, int or int64_t, options)` | `FieldResult<...>`: the parsed value on a valid commit (`committed`), `rejected` when the text is not a number, `cancelled` on Esc |
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

### Scrolling

A vertical scroll container takes `.clip = scroll_clip(ui.widgets().scroll(), id)`.
`SmoothScroll` eases the hovered container toward its target, 40 px per wheel
notch times the UI scale, and keeps it inside the content. Clay still tracks
the containers but gets a zero wheel delta. The F11 debug view scrolls the
same way. Content that must stay visible, such as the prefab filter, goes
above the container, not inside it.

Clay tracks at most 10 clip containers (`scrollContainerDatas`, fixed when
Clay starts); one more corrupts its memory. The editor stays under it:

- only the leaf lists scroll: prefabs, scenes, instances, console and the
  inspector body; panels and bars never clip;
- an unfocused text field shortens its text with `...` (`fit_text`) and
  draws it as a floating child, which does not widen the field; only the
  focused field clips;
- `Layout.SelectionAndDebugViewStayWithinClayLimits` fails if a change goes
  over.

In a small window the sidebar and the inspector shrink to 180 and 240 px and
the console to 60 px, so the console and the status bar stay visible down to
about 290 px of height. The console height comes from the window height only
(`bottom_panel_height`): a growing console would trade space with the
workspace, and Clay weighs that trade by content, so selecting an instance
with many fields would move every panel.

## Code layout

```
editor/
  assets/fonts/   JetBrains Mono, copied next to the executable
  src/action/     Action, Editor, EditorState: no Clay, no raylib
  src/cli/        command line (clap)
  src/clay/       Clay context
  src/text/       TextEdit, fuzzy matching, script locations in console lines: no UI
  src/ui/         layout, sections, widgets, widget state, command table
  src/viewport/   editor camera, snapping, scene layout and hit testing
  src/play/       Play: the played scene file and the game process
  src/exporter/   Export: plan and copy the runtime and the game folder
  src/settings/   settings.json: path, parsing, validation
  src/project/    new game folders and prefab scripts
  src/assets/     the asset list of the Assets tab
  src/render/     geometry and scissor helpers for the renderer
  src/raylib/     window, input, Clay renderer, font cache: the only raylib code
```

Everything outside `src/raylib/` builds into `editor-core` and is tested
headless in `tests/editor/` (`editor_add_test`).
