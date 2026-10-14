#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>

#include "engine/core/Entity.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/Vec2.hpp"
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
     * Registers core::Position and core::Velocity in @p world if they are not registered yet.
     *
     * @param world Entity storage. It must outlive this runtime, or be replaced by another call.
     */
    void attach_world(core::EntityManager& world);

    /**
     * @brief Creates an entity from a declaration registered by load_scripts().
     *
     * The entity gets a core::Position and a core::Velocity, initialised from the native fields
     * of its prefab (`x`, `y`, `vx`, `vy`, 0 when absent), and its own table for custom fields.
     * Scripts reach it through a handle whose fields resolve, in order, to native components,
     * the entity's own fields, then its prefab.
     *
     * @param type Declared entity name.
     * Calls the entity's `on_spawn(self)` callback once it is ready; an error raised by it is
     * logged and the entity is kept.
     *
     * @param position Starting position. Overrides the prefab's `x` and `y` when given.
     * @return The new entity, or std::nullopt if no world is attached, @p type is unknown or
     * declared as a system, or a prefab native field has the wrong type. The error is logged.
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
