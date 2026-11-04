# Editor backend

`engine-editor` is the model behind the editor: it opens a game folder, lists
its prefabs and scenes, holds the open scene, applies edits with undo and redo,
and saves. It has no window and no UI: a front (ImGui, Qt, a test...) shows
what the getters return and calls the commands.

```cmake
target_link_libraries(my-editor PRIVATE engine::editor)
```

```cpp
#include "engine/editor/EditorSession.hpp"
```

Everything lives in `namespace engine::editor`. The headers in
`engine/include/engine/editor/` are the reference; this page is the tour.

## Opening a game folder

```cpp
std::optional<EditorSession> session = EditorSession::open("path/to/game");
if (!session) {
    show_error("not a game folder, see the log");
}
if (!session->scripts_loaded()) {
    show_warning("some scripts failed to load, see the log");
}
```

`open()` loads the declarations of `scripts/` and reads `game.json`. It does
**not** start the game: no `on_start`, no start scene. It only fails when the
path is not a folder. A broken script is logged and leaves
`scripts_loaded()` false, but the rest is still usable. No scene is open
afterwards.

| Getter | Returns |
|---|---|
| `game()` | The game folder. |
| `prefabs()` | Entity types that can be placed, sorted. Systems are not listed. |
| `scenes()` | Scene names (`scenes/<name>.json`), sorted. |
| `start_scene()` | `start_scene` of `game.json`, or `std::nullopt`. |
| `runtime()` | The `script::ScriptRuntime`, for `script_fields(type)`, `fields()` and `find_declaration(name)`. |
| `default_value(type, key)` | What an instance of `type` has for `key` without an override. |

## Commands and `CommandResult`

Every command returns a `CommandResult`:

```cpp
struct CommandResult {
    bool ok;
    std::string message;
};
```

- `ok == false`: the command was **refused** and changed nothing. `message`
  says why, in a sentence ready to show to the user.
- `ok == true`: done. `message` is usually empty; when it is not, it is a note
  worth showing (for example, `open_scene()` counts what it skipped).

Commands never throw. Details of file errors go to the log; the message then
ends with "see the log".

## The open scene

One scene is open at a time. Three things describe it:

- **The document** (`document()`) is the source of truth: a `SceneDocument`
  with the instances, in order, and the unknown instances kept from the file.
  Each instance has an `InstanceId`, stable for the whole session (it survives
  undo and redo) but never saved.
- **The edit world** (`world()`) is an `EntityManager` built from the
  document. It is rebuilt after every change, so draw it fresh every frame and
  never keep its `Entity` values across commands. `entity_of(id)` and
  `instance_at(entity)` map between the two (for picking with the mouse).
- **Problems** (`problems()`) list what the edit world could not build:
  `Problem{instance, field, message}`, with `field` empty when the whole
  instance was skipped. Show them next to the instance or field.

```cpp
for (const DocumentInstance& entry : session->document().instances) {
    draw_row(entry.id, entry.instance.name, entry.instance.type, entry.instance.position);
    if (auto entity = session->entity_of(entry.id)) {
        highlight_if_selected(*entity, session->selection() == entry.id);
    }
}
for (const Problem& problem : session->problems()) {
    show_problem(problem.instance, problem.field, problem.message);
}
```

!!! warning "No Lua runs in the editor"
    The edit world is built from the declarations only: no `on_start`,
    `on_spawn`, `update`, `on_collide`, `on_destroy`, and no system. What must
    be visible in the editor (sprite, colour, size...) belongs in the
    declaration, not in `on_spawn`.

## Edits

| Command | Does | Refused when |
|---|---|---|
| `place(type, position)` | Adds an instance and **selects** it. | No scene is open, `type` is not a declared entity, the position is not finite. |
| `move(id, position)` | Moves an instance. | Unknown id, position not finite. |
| `remove(id)` | Removes an instance; it leaves the selection. | Unknown id. |
| `rename(id, name)` | Sets the label; `""` removes it. | Unknown id. |
| `set_field(id, key, value)` | Overrides a field (see below). | Unknown id, `Position.pos`, unknown or non-editable key, wrong kind, value the component refuses. |
| `reset_field(id, key)` | Back to the prefab's value. | Unknown id. |
| `reset_all(id)` | Removes every override. | Unknown id. |
| `undo()` / `redo()` | Walks the history. | Nothing to undo / redo. |

`can_undo()` and `can_redo()` tell whether to grey out the buttons. Up to
`EditorSession::history_limit` (100) edits are kept; a new edit clears redo.
An edit that changes nothing (moving to the same place...) succeeds without
being recorded.

### Fields

A field key is either:

- `"Component.field"`, an **editable** field of the registry
  (`session->runtime().fields()`), e.g. `"Sprite.color"`, `"Rotation.degrees"`;
- `"Script.<name>"`, a script field of the instance's type
  (`session->runtime().script_fields(type)`), e.g. `"Script.health"`.

`set_field()` checks the value against the component before recording it, so
a refused value never reaches the document. Setting a value equal to
`default_value(type, key)` removes the override instead of storing it.
`Position.pos` is refused: use `move()`.

```cpp
const auto result = session->set_field(id, "Sprite.color",
                                       engine::core::Rgba{.r = 255, .g = 0, .b = 0, .a = 255});
if (!result.ok) {
    show_error(result.message);
}
```

## Selection

`select(id)` selects an instance, `select(std::nullopt)` clears it,
`selection()` reads it. The selection is not part of undo; it is cleared when
its instance disappears (removed, undone, scene closed).

## Scene commands

| Command | Does | Refused when |
|---|---|---|
| `create_scene(name)` | Writes an empty `scenes/<name>.json` and opens it. | Invalid name, scene exists, unsaved changes. |
| `open_scene(name)` | Reads the scene into the document; clears history and selection. | Invalid or unknown name, file rejected (bad JSON, wrong version...), unsaved changes. |
| `duplicate_scene(from, to)` | Copies the **saved** file; does not open the copy. | Invalid name, unknown `from`, existing `to`. |
| `rename_scene(from, to)` | Renames the file; follows it in `game.json` and in the open scene. | Invalid name, unknown `from`, existing `to`. |
| `delete_scene(name)` | Deletes the file; closes it if open. | Unknown scene, start scene, open scene with unsaved changes. |
| `set_start_scene(name)` | Writes `start_scene` in `game.json`, keeping its other keys. | Invalid or unknown name. |
| `save()` | Writes the document to the open scene's file. | No scene open, write failed. |
| `discard_changes()` | Back to the last saved or opened document; clears history. | No scene open. |

Scene names follow the rules of [Game folder and scenes](game-folder.md):
1 to 63 letters, digits, `_` or `-`.

`unsaved()` is true while the document differs from the last saved or opened
one, so undoing back to it clears the flag. A command that would lose unsaved
changes (`open_scene`, `create_scene`, deleting the open scene) is refused:
ask the user, then call `save()` or `discard_changes()` and try again.

```cpp
if (session->unsaved() && ask_user("Save changes?")) {
    session->save();
} else {
    session->discard_changes();
}
session->open_scene("stage2");
```

Instances whose type no script declares are kept as they were in the file and
written back by `save()`. Instances that are malformed in the file (no type,
bad position...) are skipped when opening, counted in the message and logged,
and are not written back.

## Not covered yet

- Changing the type of an instance (#151).
- Play and stop from the editor (#157).

## Threading

`EditorSession` is not thread safe: call it from one thread, usually the UI
thread. It is movable, not copyable.
