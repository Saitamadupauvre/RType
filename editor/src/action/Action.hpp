#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "engine/core/FieldRegistry.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/editor/SceneDocument.hpp"

namespace editor {

using engine::editor::InstanceId;

namespace edit {

struct Place {
    std::string type;
    engine::core::Vec2 position;
};

struct Move {
    InstanceId id;
    engine::core::Vec2 position;
};

struct Remove {
    InstanceId id;
};

struct Rename {
    InstanceId id;
    std::string name;
};

struct SetField {
    InstanceId id;
    std::string key;
    engine::core::FieldValue value;
};

struct ResetField {
    InstanceId id;
    std::string key;
};

struct ResetAll {
    InstanceId id;
};

} // namespace edit

using Edit = std::variant<edit::Place, edit::Move, edit::Remove, edit::Rename, edit::SetField,
                          edit::ResetField, edit::ResetAll>;

enum class Overlay : std::uint8_t { hitboxes, names };
enum class Tool : std::uint8_t { select, move, pan };
enum class BottomTab : std::uint8_t { console, problems };

namespace command {

struct Select {
    std::optional<InstanceId> id;
};

struct Undo {};
struct Redo {};
struct Save {};
struct DiscardChanges {};

struct CreateScene {
    std::string name;
};

struct OpenScene {
    std::string name;
};

struct DuplicateScene {
    std::string from;
    std::string to;
};

struct RenameScene {
    std::string from;
    std::string to;
};

struct DeleteScene {
    std::string name;
};

struct SetStartScene {
    std::string name;
};

struct SetSnap {
    bool enabled = false;
};

struct SetGridSize {
    float size = 0.F;
};

struct SetOverlay {
    Overlay overlay = Overlay::hitboxes;
    bool visible = false;
};

struct SetTool {
    Tool tool = Tool::select;
};

struct ShowTab {
    BottomTab tab = BottomTab::console;
};

struct Play {};
struct Pause {};
struct Stop {};

} // namespace command

using Command = std::variant<command::Select, command::Undo, command::Redo, command::Save,
                             command::DiscardChanges, command::CreateScene, command::OpenScene,
                             command::DuplicateScene, command::RenameScene, command::DeleteScene,
                             command::SetStartScene, command::SetSnap, command::SetGridSize,
                             command::SetOverlay, command::SetTool, command::ShowTab, command::Play,
                             command::Pause, command::Stop>;

using Action = std::variant<Edit, Command>;

} // namespace editor
