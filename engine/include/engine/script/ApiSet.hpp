#pragma once

#include <string_view>

#include "engine/script/Export.hpp"

namespace engine::script {

class RuntimeContext;
class ScriptRuntime;

/**
 * @brief A group of Lua functions installed on a ScriptRuntime on top of the core API.
 *
 * Sets are chosen with ScriptRuntimeBuilder::with() and owned by the runtime they are built
 * into: a function of a set that was not added does not exist in that runtime's sandbox, so
 * calling it raises an ordinary Lua error. Reach an installed set with ScriptRuntime::api().
 *
 * @note Not thread safe. Sets are only created by ScriptRuntimeBuilder.
 */
class ENGINE_SCRIPT_EXPORT ApiSet {
public:
    virtual ~ApiSet();

    ApiSet(const ApiSet&) = delete;
    ApiSet& operator=(const ApiSet&) = delete;
    ApiSet(ApiSet&&) = delete;
    ApiSet& operator=(ApiSet&&) = delete;

    /**
     * @brief Name of the entity declaration section this set reads, or an empty view.
     *
     * A runtime merges the declaration section of that name (for example `server = { ... }`) into
     * every prefab and drops the other sections. At most one set of a runtime reads a section.
     */
    [[nodiscard]] virtual std::string_view section() const noexcept;

protected:
    ApiSet() = default;

private:
    friend class ScriptRuntime;

    virtual void install(RuntimeContext& context) = 0;
};

} // namespace engine::script
