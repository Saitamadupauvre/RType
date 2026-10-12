#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>

#include "engine/script/Declaration.hpp"
#include "engine/script/Export.hpp"

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

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace engine::script
