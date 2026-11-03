#pragma once

#include <concepts>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <typeinfo>
#include <vector>

#include "engine/core/Entity.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/script/ApiSet.hpp"
#include "engine/script/Declaration.hpp"
#include "engine/script/Export.hpp"
#include "engine/script/Instance.hpp"
#include "engine/script/Scene.hpp"
#include "engine/script/ScriptField.hpp"
#include "engine/script/ScriptRuntimeBuilder.hpp"

namespace engine::script {

/**
 * @brief Owns one sandboxed Lua state and runs untrusted scripts in it.
 *
 * Only the base, math, string, table and coroutine libraries are available. The io and os
 * libraries are never opened, and dofile and loadfile are removed, so scripts cannot reach the
 * file system or run commands. Only Lua source text is accepted: precompiled bytecode is
 * rejected, both by this class and by the load function seen by scripts.
 *
 * Every script runs in protected mode: a syntax or runtime error is logged at Level::Error with
 * the chunk name and line (for example "scripts/bydo.lua:14: attempt to index a nil value") and
 * the runtime stays usable.
 *
 * The runtime itself only holds the core API: the sandbox, `entity`, `system`, `require` and the
 * declarations `sounds`, `musics` and `event`. Everything else comes from the API sets chosen
 * with builder(), such as ServerApi or ClientApi; the functions of a set that was not added do
 * not exist in the sandbox.
 *
 * @note Not thread safe. Not copyable. A moved-from runtime can only be destroyed or assigned.
 */
class ENGINE_SCRIPT_EXPORT ScriptRuntime {
public:
    /**
     * @brief Starts building a runtime.
     *
     * `ScriptRuntime::builder().build()` creates a runtime with the core API only.
     */
    [[nodiscard]] static ScriptRuntimeBuilder builder();

    ~ScriptRuntime();

    ScriptRuntime(const ScriptRuntime&) = delete;
    ScriptRuntime& operator=(const ScriptRuntime&) = delete;
    ScriptRuntime(ScriptRuntime&&) noexcept;
    ScriptRuntime& operator=(ScriptRuntime&&) noexcept;

    /**
     * @brief Returns an API set added with ScriptRuntimeBuilder::with().
     *
     * The reference stays valid until the runtime is destroyed, even if the runtime is moved.
     *
     * @tparam Set ServerApi, ClientApi or EditorApi.
     * @throws std::out_of_range If @p Set was not added to this runtime.
     */
    template <std::derived_from<ApiSet> Set> [[nodiscard]] Set& api() {
        return static_cast<Set&>(find_api(typeid(Set)));
    }

    /**
     * @brief Loads and runs a Lua file.
     *
     * @param path File to run. Error messages use this path as the chunk name.
     * @return true if the file was loaded and ran to completion; false if it is missing, has a
     * syntax error or raised a runtime error. The error is logged.
     * @note Never throws.
     */
    bool run_file(const std::filesystem::path& path) noexcept;

    /**
     * @brief Runs a piece of Lua code.
     *
     * @param code Lua source to run.
     * @param chunk_name Name shown in error messages in place of a file path.
     * @return true if the code ran to completion; false on a syntax or runtime error, which is
     * logged.
     * @note Never throws.
     */
    bool run_string(std::string_view code, std::string_view chunk_name) noexcept;

    /**
     * @brief Loads every declaration file of a script folder, then calls `on_start()` from its
     * `main.lua`.
     *
     * Every `.lua` file under @p root is run recursively, in path order, each in its own
     * environment whose missing names fall back to the globals. Files under `root/lib/` are
     * skipped: they are modules reached with `require("name")`, which loads `lib/name.lua` once
     * and caches its result. `main.lua` at the root runs last.
     *
     * A file declares either one short form (`entity "name"` alone, the whole file becomes the
     * prefab) or any number of table forms (`entity "name" { ... }`), never both. A file breaking
     * that rule, failing to run, or reusing a name already declared is rejected as a whole: none
     * of its declarations are registered and the error is logged with both file paths for a
     * duplicate. Other files keep loading. Files declaring nothing and reserved names are logged at
     * Level::Warn.
     *
     * A declaration may hold `server = { ... }` and `client = { ... }` tables. The section read by
     * an API set of this runtime is merged into the prefab; both are then removed from it, so a
     * core-only runtime reads neither. A section that is not a table, or that sets a name the
     * declaration already sets, rejects the file. `main.lua` follows the same rule with its
     * globals: `server = { on_start = ... }` defines `on_start()` on the server only.
     *
     * @param root Script folder. Paths in messages and in DeclarationInfo are relative to it.
     * @return true if every file loaded, `main.lua` exists and `on_start()` did not fail; false
     * otherwise. Every failure is logged.
     * @note Can be called once per runtime; later calls fail. Never throws.
     */
    bool load_scripts(const std::filesystem::path& root) noexcept;

    /**
     * @brief Loads a game folder: its scripts, then its start scene, then calls `on_start()`.
     *
     * A game folder holds `game.json`, `scripts/` and `scenes/`. The scripts in `scripts/` are
     * loaded as load_scripts() does. If `game.json` names a `start_scene`, the file
     * `scenes/<start_scene>.json` is read with read_scene() and its instances are spawned with
     * spawn_instances() before `on_start()` runs, so `on_start()` already sees them. The scene is
     * only spawned when a world is attached: a client gets these entities from the server.
     *
     * A missing `game.json`, or one without `start_scene`, loads the scripts alone. A `game.json`
     * that is not a JSON object, or whose `start_scene` is not a scene name (1 to 63 letters,
     * digits, `_` or `-`), is logged and no scene is spawned. A start scene that cannot be read
     * is logged and nothing of it is spawned. In every case the scripts still load and
     * `on_start()` still runs.
     *
     * @param game Game folder. Every file is found relative to it, never the working directory.
     * @return true if the scripts loaded as for load_scripts(), `game.json` is valid and the start
     * scene, if any, was read; false otherwise. Skipped instances and overrides are logged but do
     * not make it fail.
     * @note Can be called once per runtime, instead of load_scripts(). Never throws.
     */
    bool load_game(const std::filesystem::path& game) noexcept;

    /**
     * @brief Reads a scene file against the loaded declarations and field registry.
     *
     * The file is untrusted. It is rejected as a whole when it cannot be read, is not valid JSON,
     * is not an object, or has no `version` equal to 1 or no `instances` array. Otherwise every
     * instance is read on its own and these are skipped and reported in SceneLoad::issues:
     * - an instance that is not an object, or whose `type` is missing or not a string, whose
     *   `name` is not a string, whose `position` is not `[x, y]`, or whose `overrides` is not an
     *   object;
     * - an instance whose type is not a declared entity: it is kept unchanged in
     *   Scene::unknown_instances, so write_scene() writes it back;
     * - an override naming an unknown or non-editable field or an unknown script field of the
     *   instance's type, or holding a value of the wrong kind
     *   or that a value of its kind cannot hold (an int above 2^63 - 1, a color channel above
     *   255...).
     *
     * Values that are of the right kind but that the component refuses (a negative hitbox size...)
     * are reported by spawn_instances(). Every issue and error is also logged.
     *
     * @param file Scene file, usually `<game>/scenes/<name>.json`.
     * @return The scene, its issues, and the error that rejected the file, if any.
     * @note Call it after load_scripts() or load_game(): before, every type is unknown. Never
     * throws.
     */
    [[nodiscard]] SceneLoad read_scene(const std::filesystem::path& file) const noexcept;

    /**
     * @brief Looks up a name registered by load_scripts().
     *
     * @param name Declared entity or system name.
     * @return Its kind and source file, or std::nullopt if no loaded file declares it.
     */
    [[nodiscard]] std::optional<DeclarationInfo> find_declaration(std::string_view name) const;

    /**
     * @brief Connects the runtime to the world whose entities scripts read and write.
     *
     * Registers in @p world every component scripts can reach and that is not registered yet:
     * the engine-core components, physics::PhysicsBody, physics::AABBCollider,
     * physics::CollidesWith and physics::Scriptable.
     *
     * @param world Entity storage. It must outlive this runtime, or be replaced by another call.
     */
    void attach_world(core::EntityManager& world);

    /**
     * @brief The component fields scripts read and write, filled with the engine-core and
     * physics fields.
     *
     * Every entity field seen by Lua goes through this registry, so other consumers such as
     * snapshots can share it.
     */
    [[nodiscard]] const core::FieldRegistry& fields() const noexcept;

    /**
     * @brief Creates an entity from a declaration registered by load_scripts().
     *
     * The entity gets a core::Position, then every component field its prefab sets through an
     * alias (`x`, `y`, `vx`, `vy`, `sprite`, `color`, `shader`, `tags`, `hitbox`, `collides_with`),
     * and its own table for custom fields. Setting `vx`, `vy` or `hitbox` on an entity without a
     * physics::PhysicsBody gives it a dynamic body without gravity or collision response.
     * Table-valued custom fields of the prefab are deep-copied into that table, so no two entities
     * share one. Scripts reach the entity through a handle whose fields resolve, in order, to
     * component aliases, methods, the entity's own fields, then its prefab.
     *
     * @param type Declared entity name.
     * Calls the entity's `on_spawn(self)` callback once it is ready; an error raised by it is
     * logged and the entity is kept.
     *
     * @param position Starting position. Overrides the prefab's `x` and `y` when given.
     * @return The new entity, or std::nullopt if no world is attached, @p type is unknown or
     * declared as a system, or a prefab component field is invalid. The error is logged.
     * @note Never throws.
     */
    std::optional<core::Entity> spawn(std::string_view type,
                                      std::optional<core::Vec2> position = std::nullopt) noexcept;

    /**
     * @brief Forgets the script data of an entity.
     *
     * Every handle to @p entity becomes stale: using it from Lua raises an error, which is logged
     * by the protected call that runs the script. Does not destroy the entity in the world.
     *
     * @param entity Entity returned by spawn(). Unknown entities are ignored.
     */
    void release(core::Entity entity) noexcept;

    /**
     * @brief Calls a method of an entity's prefab, as `self:method()` would from Lua.
     *
     * The call is protected: an error raised by the method is logged with its file and line.
     *
     * @param entity Entity returned by spawn().
     * @param method Name of a function stored in the entity's prefab.
     * @return true if the method ran to completion; false if the entity is destroyed or released,
     * has no such method, or the method raised an error. Errors are logged; a missing method is
     * not, since callbacks are optional.
     * @note Never throws.
     */
    bool call(core::Entity entity, std::string_view method) noexcept;

    /**
     * @brief Lists the names of the declared entity types, in ascending order.
     *
     * The order is stable for a given set of scripts, so two processes loading the same scripts
     * get the same list.
     */
    [[nodiscard]] std::vector<std::string> entity_types() const;

    /**
     * @brief Lists the Lua free fields of an entity type that instances can override.
     *
     * Inferred once, when the declaration loads, from the defaults it gives (see ScriptField for
     * the rules). Instances store them under the key `"Script.<name>"`, e.g. `"Script.health"`.
     * A field declared only inside a `server = { ... }` or `client = { ... }` section is not
     * listed, so every API set sees the same list.
     *
     * @param type Declared entity name.
     * @return The fields in ascending name order, empty for an unknown type or a system.
     */
    [[nodiscard]] std::vector<ScriptField> script_fields(std::string_view type) const;

    /**
     * @brief Lists the sound effects declared with `sounds { ... }`, in ascending order.
     *
     * Scripts declare sounds while load_scripts() runs, for example in `main.lua`:
     * `sounds { "shot.wav", "explosion.wav" }`. A name is a file name of 1 to 63 letters, digits,
     * `_`, `-` or `.`, not starting with `.`; the host decides which folder it lives in. A call
     * with an invalid or already declared name raises a Lua error and declares none of its names.
     * The order is stable for a given set of scripts.
     */
    [[nodiscard]] std::vector<std::string> sound_names() const;

    /**
     * @brief Lists the music tracks declared with `musics { ... }`, in ascending order.
     *
     * Same rules as sound_names().
     */
    [[nodiscard]] std::vector<std::string> music_names() const;

    /**
     * @brief Lists the textures declared with `textures { ... }`, in ascending order.
     *
     * Scripts declare textures while load_scripts() runs, for example
     * `textures { "sprites/ship.png", "sprites/boss/core.png" }`. The `sprite` of every declared
     * entity is declared on its own. A name is 1 to 63 bytes of `/`-separated folders and a file,
     * each made of letters, digits, `_`, `-` or `.` and not starting with `.`. Declaring a name
     * twice is allowed; an invalid name raises a Lua error and declares none of its call's names.
     * Setting an entity field to a texture missing from this list raises a Lua error. The order
     * is stable for a given set of scripts.
     */
    [[nodiscard]] std::vector<std::string> texture_names() const;

    /**
     * @brief Lists the shaders declared with `shaders { ... }`, in ascending order.
     *
     * Works like texture_names(): the `shader` of every declared entity is declared on its own,
     * and setting an entity's `shader` to a name missing from this list raises a Lua error. A
     * shader name has no extension, such as `shaders/space`.
     */
    [[nodiscard]] std::vector<std::string> shader_names() const;

    /**
     * @brief Lists the events declared with `event "name" { key = "kind", ... }`, in ascending
     * order.
     *
     * Events are declared while the scripts load, e.g. `event "explosion" { at = "vec2",
     * size = "float" }`. A name is 1 to 63 letters, digits, `_`, `-` or `.`, starting with a
     * letter. Keys are Lua identifiers of 1 to 63 characters; kinds are `float`, `int`, `bool`,
     * `string`, `vec2`, `color` or `asset`; an event has at most `max_event_arguments` keys. An
     * invalid declaration, a name declared twice or a declaration made after loading raises a
     * Lua error.
     */
    [[nodiscard]] std::vector<std::string> event_names() const;

    /**
     * @brief Lists every argument key of every declared event, once each, in ascending order.
     */
    [[nodiscard]] std::vector<std::string> event_keys() const;

    /**
     * @brief Returns the declared type of an entity spawned by this runtime.
     *
     * @return The type name, or std::nullopt for entities this runtime did not spawn or already
     * released.
     */
    [[nodiscard]] std::optional<std::string> entity_type(core::Entity entity) const;

    /**
     * @brief Returns the label of an entity spawned by this runtime.
     *
     * @return The name, empty if none was given, or std::nullopt for entities this runtime did not
     * spawn or already released.
     */
    [[nodiscard]] std::optional<std::string> entity_name(core::Entity entity) const;

    /**
     * @brief Labels an entity spawned by this runtime, as the editor names scene entities.
     *
     * Names are free text: they need not be unique and scripts never see them.
     *
     * @return false, changing nothing, for entities this runtime did not spawn or already released.
     */
    bool set_entity_name(core::Entity entity, std::string name);

    /**
     * @brief Captures the attached world as one Instance per live entity spawned by this runtime.
     *
     * Entities marked with `self:destroy()` and entities this runtime did not spawn are left out.
     * Instances come in ascending entity id order. An instance stores every editable field of the
     * entity, except `Position.pos` which is its position, whose value differs from the type's
     * default: the value the field gets from the prefab's component fields (`sprite`, `hitbox`...)
     * before `on_spawn` runs. A field of a component the prefab does not give is always stored.
     * Each script field (see script_fields()) whose value differs from its prefab default is
     * stored as `"Script.<name>"`; one holding a value of another kind is left out and logged at
     * Level::Warn.
     *
     * @return The instances, empty if no world is attached. Save them with write_scene().
     * @note An entity spawned by another entity's `on_spawn` is captured like any other, so
     * spawning the instances back spawns it twice.
     */
    [[nodiscard]] std::vector<Instance> capture_instances();

    /**
     * @brief Spawns one entity per instance into the attached world.
     *
     * Each instance is spawned as spawn() would at its position and named, then `on_spawn` runs,
     * then its overrides are written through fields(), so they win over what `on_spawn` set.
     * Entities already in the world are kept.
     *
     * @return What was skipped: an instance whose type is unknown or declared as a system, or
     * that cannot be spawned; an override naming an unknown or non-editable field or an unknown
     * script field, or holding a value of the wrong kind or out of range. The other instances and
     * overrides are still
     * applied. Every issue is also logged at Level::Warn.
     * @note Never throws.
     */
    std::vector<InstanceIssue> spawn_instances(std::span<const Instance> instances) noexcept;

    /**
     * @brief Advances every live entity by one tick.
     *
     * In ascending id order, adds @p dt to each entity's `age`, then calls its `update(self, dt)`
     * callback if it has one. Entities marked with `self:destroy()` are skipped. Entities spawned
     * during the pass are first updated on the next call. Then calls `update(dt)` from
     * `main.lua` if it defines one. An error raised by a callback is logged with its file and line
     * and the other callbacks still run.
     *
     * @param dt Fixed time step in seconds.
     * @note Never throws.
     */
    void update(float dt) noexcept;

    /**
     * @brief Reports that the hitboxes of two entities overlap.
     *
     * Each entity whose `collides_with` list holds one of the other's tags gets its
     * `on_collide(self, other)` callback called, @p first before @p second. Both callbacks run
     * even if the first one destroys either entity. Nothing happens if either entity is unknown,
     * dead or already marked with `self:destroy()` when the call starts, so an entity destroyed
     * by a collision gets no other collision in the same tick. Setting `hitbox = { w = .., h = ..
     * }` gives an entity an axis-aligned collider anchored at its top-left `x`, `y`; the host
     * detects the overlaps, typically with engine::physics::PhysicsSystem, and calls this function
     * for each of them. An error raised by a callback is logged.
     *
     * @param first Entity spawned by this runtime.
     * @param second Another entity spawned by this runtime.
     * @note Never throws.
     */
    void collide(core::Entity first, core::Entity second) noexcept;

    /**
     * @brief Removes every entity marked with `self:destroy()`.
     *
     * For each marked entity, calls its `on_destroy(self)` callback, forgets its script data and
     * kills it in the world. Entities marked during these callbacks are removed in the same call.
     *
     * @return The number of entities removed.
     * @note Never throws.
     */
    std::size_t flush_destroyed() noexcept;

private:
    friend class ScriptRuntimeBuilder;

    struct Impl;

    explicit ScriptRuntime(std::vector<std::unique_ptr<ApiSet>> sets);
    [[nodiscard]] ApiSet& find_api(const std::type_info& type) const;

    std::unique_ptr<Impl> _impl;
};

} // namespace engine::script
