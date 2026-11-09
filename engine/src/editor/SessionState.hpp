#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "History.hpp"
#include "engine/core/ecs/EntityManager.hpp"
#include "engine/editor/CommandResult.hpp"
#include "engine/editor/EditorSession.hpp"
#include "engine/editor/SceneDocument.hpp"
#include "engine/script/ScriptRuntime.hpp"

namespace engine::editor {

struct EditorSession::Impl {
    explicit Impl(std::filesystem::path folder);

    std::filesystem::path game;
    script::ScriptRuntime runtime;
    bool scripts_loaded = false;
    std::vector<ScriptMessage> script_messages;
    std::optional<std::string> start_scene;
    std::optional<std::string> open_scene;
    SceneDocument document;
    SceneDocument saved;
    History history{history_limit};
    std::optional<InstanceId> selection;
    std::uint64_t next_id = 1;
    std::unique_ptr<core::EntityManager> world = std::make_unique<core::EntityManager>();
    std::vector<std::optional<core::Entity>> entities;
    std::vector<Problem> problems;

    InstanceId new_id() noexcept { return InstanceId{next_id++}; }
    [[nodiscard]] bool unsaved() const { return open_scene.has_value() && document != saved; }
    [[nodiscard]] bool scene_exists(std::string_view name) const;
    [[nodiscard]] std::optional<CommandResult> refuse_unsaved() const;

    void load_scripts();
    void rebuild();
    void forget_missing_selection() noexcept;
    void show(std::optional<std::string> name, SceneDocument opened);
    void close();
    CommandResult commit(SceneDocument next);
};

[[nodiscard]] std::optional<CommandResult> refuse_scene_name(std::string_view name);

} // namespace engine::editor
