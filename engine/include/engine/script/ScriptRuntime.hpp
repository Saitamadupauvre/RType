#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/core/Entity.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/script/AudioCommand.hpp"
#include "engine/script/Declaration.hpp"
#include "engine/script/Export.hpp"
#include "engine/script/InputState.hpp"

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
 * @note Not thread safe. Not copyable or movable.
 */
class ENGINE_SCRIPT_EXPORT ScriptRuntime {
public:
    /**
     * @brief Creates a fresh Lua state with the safe libraries opened.
     *
     * @throws std::bad_alloc If the Lua state cannot be allocated.
     */
    ScriptRuntime();
    ~ScriptRuntime();

    ScriptRuntime(const ScriptRuntime&) = delete;
    ScriptRuntime& operator=(const ScriptRuntime&) = delete;
    ScriptRuntime(ScriptRuntime&&) = delete;
    ScriptRuntime& operator=(ScriptRuntime&&) = delete;

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
     * @param root Script folder. Paths in messages and in DeclarationInfo are relative to it.
     * @return true if every file loaded, `main.lua` exists and `on_start()` did not fail; false
     * otherwise. Every failure is logged.
     * @note Can be called once per runtime; later calls fail. Never throws.
     */
    bool load_scripts(const std::filesystem::path& root) noexcept;

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
     * The entity gets a core::Position and a core::Velocity, then every component field its
     * prefab sets through an alias (`x`, `y`, `vx`, `vy`, `sprite`, `color`, `tags`, `hitbox`,
     * `collides_with`), and its own table for custom fields. Scripts reach the entity through a
     * handle whose fields resolve, in order, to component aliases, methods, the entity's own
     * fields, then its prefab.
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
     * @brief Sets the actions an entity reads through `self.input` until the next call.
     *
     * Entities never given an input read a neutral one: every axis is 0, nothing is pressed.
     *
     * @param entity Entity returned by spawn(). Unknown entities are ignored.
     * @param input Actions for the coming ticks.
     */
    void set_input(core::Entity entity, InputState input) noexcept;

    /**
     * @brief Registers a connected player and calls `on_player_joined(player)` from `main.lua`.
     *
     * The player gets the lowest free slot, starting at 0, and a neutral input. Scripts give it
     * entities by setting their `owner` field; those entities then read the player's input
     * through `self.input`. The callback is optional and protected: an error is logged.
     *
     * @param player_id Identifier chosen by the caller, unique among connected players.
     * @return true if the player was added and the callback, when defined, ran to completion;
     * false if @p player_id is already connected (logged, nothing changes) or the callback failed.
     * @note Never throws.
     */
    bool player_joined(std::uint32_t player_id) noexcept;

    /**
     * @brief Calls `on_player_left(player)` from `main.lua`, then forgets the player.
     *
     * Without that callback, every entity the player owns is marked for destruction and removed
     * by the next flush_destroyed(). With it, the script decides; entities still owned afterwards
     * read a nil `owner` and a neutral input.
     *
     * @param player_id Player registered by player_joined().
     * @return true if the player was removed and the callback, when defined, ran to completion;
     * false if @p player_id is unknown (logged) or the callback failed. The player is removed
     * even when the callback fails.
     * @note Never throws.
     */
    bool player_left(std::uint32_t player_id) noexcept;

    /**
     * @brief Sets the actions read by every entity owned by a player until the next call.
     *
     * @param player_id Player registered by player_joined(). Unknown players are ignored.
     * @param input Actions for the coming ticks.
     */
    void set_player_input(std::uint32_t player_id, InputState input) noexcept;

    /**
     * @brief Lists the names of the declared entity types, in ascending order.
     *
     * The order is stable for a given set of scripts, so two processes loading the same scripts
     * get the same list.
     */
    [[nodiscard]] std::vector<std::string> entity_types() const;

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
     * @brief Returns the audio requests queued by scripts since the last call, in call order,
     * and empties the queue.
     *
     * Scripts queue them with `play(name)`, `play(name, entity)`, `music(name)` and
     * `music(nil)`. The runtime plays nothing itself: the host plays the commands or sends them
     * to clients, typically once per tick. An undeclared name or an argument of the wrong type
     * raises a Lua error, logged by the protected call that runs the script. At most
     * 64 commands are kept between two calls; later ones are dropped and logged once.
     *
     * @note Never throws.
     */
    [[nodiscard]] std::vector<AudioCommand> take_audio_commands() noexcept;

    /**
     * @brief Returns the declared type of an entity spawned by this runtime.
     *
     * @return The type name, or std::nullopt for entities this runtime did not spawn or already
     * released.
     */
    [[nodiscard]] std::optional<std::string> entity_type(core::Entity entity) const;

    /**
     * @brief Returns the connected player that owns an entity.
     *
     * @return The owner's id, or std::nullopt if the entity is unknown, has no owner, or its
     * owner has left.
     */
    [[nodiscard]] std::optional<std::uint32_t> entity_owner(core::Entity entity) const;

    /**
     * @brief Advances every live entity by one tick.
     *
     * In ascending id order, adds @p dt to each entity's `age`, then calls its `update(self, dt)`
     * callback if it has one. Entities marked with `self:destroy()` are skipped. Entities spawned
     * during the pass are first updated on the next call. An error raised by a callback is logged
     * with its file and line and the other entities still run.
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
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace engine::script
