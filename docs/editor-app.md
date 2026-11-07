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
| Viewport | The scene, drawn like the game, the grid and the selection | Select, move, pan, zoom |
| Inspector | Name, type, position and fields of the selection | Rename, move, edit and reset fields |
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

## Viewport

The viewport draws the open scene through an editor camera, separate from the
game camera. Instances are laid out like the game's renderer: Position is the
top-left corner, a sprite takes its texture size times Scale, an entity
without a texture is a 64 px square tinted by its color, and Layer `z_index`
orders the drawing. Entities without a Sprite, which the game does not draw,
show as a crossed outline so they can still be selected. Textures load from
`<game>/assets/`; a missing one is drawn as the tinted square.

| Input | Effect |
|---|---|
| Click an instance | Select it (the topmost one when they overlap) |
| Click empty space | Clear the selection |
| Drag with the Move tool (G) | Move the instance; one `Move` on release, undoable |
| Alt while dragging | Drop without snapping |
| Middle drag, or drag with the Pan tool (H) | Pan |
| Wheel | Zoom around the cursor (0.1x to 10x) |
| Drag a prefab from PREFABS into the viewport | Place it under the cursor, snapped (`Place`) |
| Click a prefab, then click in the viewport | Same; the prefab stays highlighted until placed, Esc or a click elsewhere cancels |
| Drag the square handle of the selection | Scale it (`SetField` on `Scale.x`, `Scale.y`) |
| Drag the round handle of the selection | Rotate it, by 15° steps with snap on (`SetField` on `Rotation.degrees`) |
| Delete | Delete the selection |

Handles only appear when the entity has the component they edit (`Scale`,
`Rotation`). Alt skips snapping for handles too.

| Overlay | Shows |
|---|---|
| Camera frame (always) | What the game shows at start: the view of the first instance with a `Camera`, or the 1920x1080 game screen from the origin |
| Names (toolbar) | Each instance's name, or its type, above it |
| Hitboxes (toolbar) | `AABBCollider` and `CircleCollider` outlines, where the physics puts them: Position plus the offset is the center of the box or circle |

With snap on, a dragged instance's top-left corner lands on the grid. Grid
lines are drawn unless they would be closer than 6 px on screen; the world
axes are brighter. The camera, the drag and the pan live in
`WidgetState::viewport()`; the renderer paints the frame's `ViewportFrame`,
passed through the Clay element's custom data.

## Inspector

The inspector shows the selected instance. `Editor` caches what it shows in
`EditorState::inspector` (`inspect_selection`) and refreshes it after every
action, so the UI never queries the session.

- **Name** and **Position** edit the instance (`Rename`, `Move`). Type is read
  only.
- **Components** lists the registry fields of the components the prefab's
  entity owns, in registration order. `Position.pos` is left out: it is the
  position above.
- **Script** lists the fields the prefab script declares.
- Each field shows its override, or the prefab value when there is none. An
  overridden field has an accent label and a Reset button (`ResetField`).
  **Reset all** (`ResetAll`) appears when the instance has overrides.

| Kind | Edited with |
|---|---|
| float, int | number field |
| bool | checkbox |
| string | text field |
| vec2 | two number fields, x and y |
| color | swatch and a hex text field: `#RRGGBB` or `#RRGGBBAA` |
| asset, list | read only for now |

Committing a value emits `SetField` only when it differs from the shown one.
Invalid input emits `ReportInvalidInput`, which logs
`<field>: expected ...` in the console and changes nothing.

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

## Code layout

```
editor/
  assets/fonts/   JetBrains Mono, copied next to the executable
  src/action/     Action, Editor, EditorState: no Clay, no raylib
  src/cli/        command line (clap)
  src/clay/       Clay context
  src/text/       TextEdit: UTF-8 caret, selection and editing, no UI
  src/ui/         layout, sections, widgets, widget state, command table
  src/viewport/   editor camera, snapping, scene layout and hit testing
  src/render/     geometry and scissor helpers for the renderer
  src/raylib/     window, input, Clay renderer, font cache: the only raylib code
```

Everything outside `src/raylib/` builds into `editor-core` and is tested
headless in `tests/editor/` (`editor_add_test`).
