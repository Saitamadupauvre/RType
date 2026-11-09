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

`script_messages()` lists what the last load of the scripts logged as a
warning or an error (a broken file, a name outside a table form, a misspelled
callback), without the `lua: ` prefix. The session reads them from the log
while it loads, so messages below the log level are not seen.

`reload_scripts()` loads `scripts/` and `game.json` again into a fresh
runtime, so new, changed and deleted scripts are seen without reopening. The
open scene, its unsaved changes, its history and the selection are kept; the
edit world is rebuilt, and instances of a type that no longer exists become
problems. It always succeeds; its message gives the prefab count, or says that
a script failed to load.

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
| `duplicate(id, offset)` | Adds a copy (name, type, overrides) at its position plus `offset`, right after it, and **selects** it. | Unknown id, offset not finite. |
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

Scene names follow the rules of [Game folders and scenes](guide/game-folder.md):
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

## Game folder C++ API

Everything is in `engine/script/Scene.hpp` and `ScriptRuntime`:

```cpp
auto runtime = engine::script::ScriptRuntime::builder()
                   .with<engine::script::ServerApi>()
                   .build();
runtime.attach_world(world);
runtime.load_game(game);

const engine::script::SceneLoad load = runtime.read_scene(game / "scenes" / "stage1.json");
if (!load.ok()) {
    show_error(load.error);
}
for (const engine::script::InstanceIssue& issue : load.issues) {
    show_problem(issue.instance, issue.field, issue.message);
}

const engine::script::Scene saved{.instances = runtime.capture_instances(),
                                  .unknown_instances = load.scene.unknown_instances};
engine::script::write_scene(game / "scenes" / "stage1.json", saved);
```

| Call | Does |
|---|---|
| `ScriptRuntime::load_game(game)` | Loads `scripts/`, spawns the start scene, calls `on_start()`. Returns `false` if the scripts, `game.json` or the start scene failed. |
| `ScriptRuntime::read_scene(file)` | Reads a scene against the loaded declarations: `SceneLoad{scene, issues, error}`. `ok()` is `false` when the file was rejected. Call it after loading the scripts, otherwise every type is unknown. |
| `ScriptRuntime::spawn_instances(instances)` | Spawns instances into the attached world, see [Engine modules](engine.md#instances). |
| `ScriptRuntime::capture_instances()` | Turns the world back into instances. |
| `ScriptRuntime::flush_scene()` | Applies a pending Lua `load_scene`; returns the scene name, or `std::nullopt` when none was requested. The server calls it every tick after `flush_destroyed()`. |
| `write_scene(file, scene)` | Saves a scene. It needs no runtime. |
| `ScriptRuntime::load_declarations(scripts)` | Loads the declarations of a scripts folder without starting the game: no `on_start()`, no start scene. Used by the editor. |
| `ScriptRuntime::preview_instances(world, instances)` | Builds each instance into `world` as `spawn_instances()` would, from the prototype only: no `on_spawn`, no Lua. Returns `Preview{entities, issues}`, `entities[i]` being the entity of instance `i` or `std::nullopt`. Issues are not logged. |
| `ScriptRuntime::default_value(type, key)` | Value an instance of `type` has for `key` (`"Component.field"` or `"Script.<name>"`) without an override; `std::nullopt` for an unknown type or key. |

`engine/script/GameFolder.hpp` holds the game folder helpers, which need no
runtime:

| Call | Does |
|---|---|
| `is_scene_name(name)` | Whether `name` is a valid scene name. |
| `scene_file(game, name)` | `<game>/scenes/<name>.json`. |
| `scripts_folder(game)` | `<game>/scripts`. |
| `read_project(game)` | Reads `game.json` into `GameProject{name, start_scene, modes}`; `std::nullopt` when it is invalid. |
| `is_mode_name(name)` | Whether `name` is `solo`, `host`, `client` or `dedicated`. |
| `write_modes(game, modes)` | Sets (or, with `std::nullopt`, removes) `modes` in `game.json`, keeping its other keys. |
| `write_start_scene(game, name)` | Sets (or, with `std::nullopt`, removes) `start_scene` in `game.json`, keeping its other keys; creates the file if needed. |
| `list_scenes(game)` | Names of the scene files of `scenes/`, sorted. |

The editor backend builds on these calls.

`InstanceIssue::instance` is the index in the file's `instances` array for
`read_scene()`, and the index in the given list for `spawn_instances()`.

Every function is `noexcept` and runs on the thread that owns the runtime.

## Threading

`EditorSession` is not thread safe: call it from one thread, usually the UI
thread. It is movable, not copyable.
