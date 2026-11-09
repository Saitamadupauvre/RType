#include "action/OpenEditor.hpp"

#include <string>
#include <system_error>
#include <utility>

#include "engine/core/log/Log.hpp"
#include "engine/editor/EditorSession.hpp"

namespace editor {

std::optional<Editor> open_editor(const std::filesystem::path& game) {
    std::error_code error;
    const std::filesystem::path folder = std::filesystem::weakly_canonical(game, error);
    auto session = engine::editor::EditorSession::open(error ? game : folder);
    if (!session) {
        return std::nullopt;
    }
    const auto start = session->start_scene();
    engine::core::log::info("editor: game {}", session->game().generic_string());
    engine::core::log::info("editor: start scene {}", start ? *start : std::string("none"));
    std::optional<Editor> editor(std::in_place, std::move(*session));
    if (start) {
        editor->handle(command::OpenScene{.name = *start});
    }
    return editor;
}

} // namespace editor
