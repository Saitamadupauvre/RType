#pragma once

#include <concepts>
#include <memory>
#include <vector>

#include "engine/script/ApiSet.hpp"
#include "engine/script/Export.hpp"

namespace engine::script {

class ScriptRuntime;

/**
 * @brief Chooses the API sets of a ScriptRuntime before it is created.
 *
 * Every runtime has the core API. A server runtime adds ServerApi, a client runtime ClientApi:
 *
 * @code
 * auto runtime = ScriptRuntime::builder().with<ServerApi>().build();
 * auto& server = runtime.api<ServerApi>();
 * @endcode
 *
 * @note Not thread safe.
 */
class ENGINE_SCRIPT_EXPORT ScriptRuntimeBuilder {
public:
    ScriptRuntimeBuilder();
    ~ScriptRuntimeBuilder();

    ScriptRuntimeBuilder(const ScriptRuntimeBuilder&) = delete;
    ScriptRuntimeBuilder& operator=(const ScriptRuntimeBuilder&) = delete;
    ScriptRuntimeBuilder(ScriptRuntimeBuilder&&) noexcept;
    ScriptRuntimeBuilder& operator=(ScriptRuntimeBuilder&&) noexcept;

    /**
     * @brief Adds an API set to the runtime being built.
     *
     * @tparam Set ServerApi, ClientApi or EditorApi.
     * @return This builder, for chaining.
     */
    template <std::derived_from<ApiSet> Set> ScriptRuntimeBuilder& with() {
        _sets.push_back(std::unique_ptr<ApiSet>(new Set()));
        return *this;
    }

    /**
     * @brief Creates the runtime with the core API and every added set.
     *
     * The builder is empty afterwards and can be reused.
     *
     * @return A runtime owning the added sets.
     * @throws std::invalid_argument If a set was added twice, or two sets read a declaration
     * section (for example ServerApi with ClientApi).
     * @throws std::bad_alloc If the Lua state cannot be allocated.
     */
    [[nodiscard]] ScriptRuntime build();

private:
    std::vector<std::unique_ptr<ApiSet>> _sets;
};

} // namespace engine::script
