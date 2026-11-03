# Game folder and scene files

A **game folder** holds everything that makes a game, apart from the engine:
its project file, its scripts, its scenes and its assets. The server runs any
game folder, and the editor opens one. The engine finds every file relative to
the game folder, never relative to the working directory.

```
<game>/
├── game.json      project file: the game's name and its start scene
├── scripts/       Lua code: main.lua and the declarations (PROJECT.md §10)
├── scenes/        one JSON file per scene
└── assets/        sprites, sounds, music, shaders
```

For R-Type the game folder is `game/` in the repository. The build copies
`game.json`, `scripts/`, `scenes/` and `assets/` next to the executables, so
the folder of the executable is the game folder.

## `game.json`

```json
{
    "name": "r-type",
    "start_scene": "stage1"
}
```

| Key | Type | Meaning |
|---|---|---|
| `name` | string | Name of the game, shown by tools. The runtime does not read it. |
| `start_scene` | scene name | Scene spawned when the server starts, before `on_start()`. Optional. |

Other keys are ignored. The file itself is optional: a game folder without
`game.json`, or whose `game.json` has no `start_scene`, starts with only what
`on_start()` spawns, as before scenes existed.

A **scene name** is 1 to 63 letters, digits, `_` or `-`. The scene `stage1`
is the file `scenes/stage1.json`. Names with `/`, `.` or spaces are refused,
so a scene name can never reach a file outside `scenes/`.

## Scene files

A scene lists the **instances** to spawn: one entry per entity, with its
declared type, its position and what it changes compared to its prefab.

```json
{
    "version": 1,
    "instances": [
        {
            "name": "boss_1",
            "type": "bydo",
            "position": [840, 300],
            "overrides": {
                "PhysicsBody.mass": 2.5,
                "Sprite.color": [255, 0, 0, 255]
            }
        },
        {
            "type": "space",
            "position": [0, 0]
        }
    ]
}
```

| Key | Required | Meaning |
|---|---|---|
| `version` | yes | Format version, `1` today. Another value rejects the file. |
| `instances` | yes | Array of instances, spawned in this order. |

Each instance:

| Key | Required | Meaning |
|---|---|---|
| `type` | yes | Name of an entity declared with `entity "name"`. |
| `position` | yes | `[x, y]`, the world position of the entity's top-left corner. |
| `name` | no | Label shown by the editor. Scripts never see it, and it need not be unique. |
| `overrides` | no | Object mapping `"Component.field"` to a value. |

### Overrides

A scene stores **only the values that differ from the prefab**. The default
value of a field is the one the declaration's native fields give it (`sprite`,
`color`, `hitbox`, `rotation`...) before `on_spawn` runs. Changing a prefab
therefore changes every instance that did not override that field.

Keys are `Component.field` names from the field registry, and only editable
fields can be overridden:

| Key | Kind | Lua field |
|---|---|---|
| `Sprite.path` | asset | `sprite` |
| `Sprite.color` | color | `color` |
| `Shader.path` | asset | `shader` |
| `Tags.labels` | list of string | `tags` |
| `Rotation.degrees` | float | `rotation` |
| `Scale.x`, `Scale.y` | float | `scale` |
| `Camera.zoom`, `Camera.follow` | float, string | `zoom`, `follow` |
| `Camera.bounds_origin`, `Camera.bounds_size` | vec2 | `bounds` |
| `AABBCollider.offset`, `AABBCollider.size` | vec2 | `hitbox` |
| `CircleCollider.offset`, `CircleCollider.radius` | vec2, float | |
| `CollidesWith.labels` | list of string | `collides_with` |
| `PhysicsBody.velocity`, `PhysicsBody.acceleration` | vec2 | `vx`, `vy` |
| `PhysicsBody.mass` | float | |
| `PhysicsBody.type`, `PhysicsBody.collision_layer`, `PhysicsBody.collision_mask` | int | |
| `PhysicsBody.resolve_collisions`, `PhysicsBody.affected_by_gravity` | bool | |

Lua free fields such as `self.health` are not stored in scenes yet.

### Values

| Kind | JSON | Example |
|---|---|---|
| float | number | `2.5` |
| int | integer, -2^63 to 2^63 - 1 | `4` |
| bool | `true` or `false` | `false` |
| string | string | `"player"` |
| asset | path under `assets/` | `"sprites/ship.png"` |
| vec2 | `[x, y]` | `[16, 8]` |
| color | `[r, g, b, a]`, each 0 to 255 | `[255, 0, 0, 255]` |
| list | array of one kind | `["enemy", "boss"]` |

## Loading

On start, the server runs `ScriptRuntime::load_game()` on its game folder:

1. Every script of `scripts/` is loaded (PROJECT.md §10.2).
2. If `game.json` names a `start_scene`, its file is read and each instance is
   spawned at its position: `on_spawn` runs, then the overrides are applied, so
   they win over what `on_spawn` set.
3. `on_start()` of `main.lua` runs and already sees the scene's entities.

Clients never spawn scenes: they receive the server's entities through
replication.

## What is reported

A scene file is untrusted input, like a packet. Nothing in it crashes the
game or throws. Every problem is logged with the file and the index of the
instance, and the rest is kept:

| Problem | Result |
|---|---|
| Missing file, invalid JSON, not an object, no `version` or another version, no `instances` array | The whole file is rejected: nothing is spawned. |
| An instance that is not an object, without a string `type`, with a `name` that is not a string, a `position` that is not `[x, y]`, or `overrides` that is not an object | That instance is skipped. |
| An instance whose type is not a declared entity (a missing script, a typo, a system) | That instance is skipped, **and kept**: saving the scene writes it back unchanged. |
| An unknown field, a value of the wrong kind, or a value its kind cannot hold (an int above 2^63 - 1, a channel above 255) | That override is skipped and the entity keeps its default. |
| A value the component refuses (a negative hitbox size, a non-finite rotation) | That override is skipped when spawning, and the entity keeps its default. |

`game.json` follows the same rule. If it is invalid, or its `start_scene` is
not a scene name or cannot be read, the error is logged, nothing of the scene
is spawned, and the scripts and `on_start()` still run.

## Saving

`write_scene()` writes indented JSON:

- known instances in order, then the unknown ones exactly as they were read;
- no `name` when it is empty, no `overrides` when there are none;
- overrides sorted by key, so saving twice gives the same file and diffs stay
  small;
- floats with the fewest digits that read back to the same value (`0.1`,
  never `0.10000000149`).

The text first goes to `<file>.tmp`, which then replaces the scene, so a
failed save never leaves a truncated file. Missing folders are created.

## C++ API

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
| `write_scene(file, scene)` | Saves a scene. It needs no runtime. |

`InstanceIssue::instance` is the index in the file's `instances` array for
`read_scene()`, and the index in the given list for `spawn_instances()`.

Every function is `noexcept` and runs on the thread that owns the runtime.
