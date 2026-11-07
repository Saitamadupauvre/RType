#pragma once

#include <string>
#include <string_view>

#include "action/EditorState.hpp"
#include "engine/editor/EditorSession.hpp"
#include "engine/script/Instance.hpp"

namespace editor {

[[nodiscard]] std::string_view instance_label(const engine::script::Instance& instance);

[[nodiscard]] std::string_view mode_label(Mode mode);

[[nodiscard]] std::string scene_title(const engine::editor::EditorSession& session);

} // namespace editor
