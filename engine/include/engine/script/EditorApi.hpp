#pragma once

#include "engine/script/ApiSet.hpp"
#include "engine/script/Export.hpp"

namespace engine::script {

class ScriptRuntimeBuilder;

/**
 * @brief API set of the editor console.
 *
 * Adds no function yet and reads no declaration section: an editor runtime only sees the core
 * API.
 *
 * @note Not thread safe. Created by ScriptRuntimeBuilder::with(), reached with
 * ScriptRuntime::api(), destroyed with its runtime.
 */
class ENGINE_SCRIPT_EXPORT EditorApi final : public ApiSet {
public:
    ~EditorApi() override;

    EditorApi(const EditorApi&) = delete;
    EditorApi& operator=(const EditorApi&) = delete;
    EditorApi(EditorApi&&) = delete;
    EditorApi& operator=(EditorApi&&) = delete;

private:
    friend class ScriptRuntimeBuilder;

    EditorApi() = default;
    void install(RuntimeContext& context) override;
};

} // namespace engine::script
