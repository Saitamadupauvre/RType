#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>

#include "engine/core/ecs/FieldRegistry.hpp"
#include "engine/core/math/Vec2.hpp"
#include "engine/editor/SceneDocument.hpp"
#include "exporter/GameExport.hpp"

namespace editor {

using engine::editor::InstanceId;

/** @brief Undoable changes to the open scene, one per user intent. */
namespace edit {

/** @brief Adds an instance of a prefab and selects it. */
struct Place {
    std::string type;
    engine::core::Vec2 position;
};

/** @brief Moves an instance; emitted once, when a drag ends. */
struct Move {
    InstanceId id;
    engine::core::Vec2 position;
};

struct Remove {
    InstanceId id;
};

/** @brief Sets the label of an instance; an empty name removes it. */
struct Rename {
    InstanceId id;
    std::string name;
};

/** @brief Overrides a field; `key` is "Component.field" or "Script.<name>". */
struct SetField {
    InstanceId id;
    std::string key;
    engine::core::FieldValue value;
};

/** @brief Gives a field back its prefab value. */
struct ResetField {
    InstanceId id;
    std::string key;
};

/** @brief Removes every override of an instance. */
struct ResetAll {
    InstanceId id;
};

/** @brief Turns an instance into another prefab, keeping the overrides the new one accepts. */
struct ChangeType {
    InstanceId id;
    std::string type;
};

} // namespace edit

/**
 * @brief A change to the open scene. Refused, with a console line, unless the editor is in
 * Mode::editing.
 */
using Edit = std::variant<edit::Place, edit::Move, edit::Remove, edit::Rename, edit::SetField,
                          edit::ResetField, edit::ResetAll, edit::ChangeType>;

enum class Overlay : std::uint8_t { hitboxes, names };
enum class Tool : std::uint8_t { select, move, pan };
enum class BottomTab : std::uint8_t { console, problems };

/**
 * @brief Everything that is not an Edit.
 *
 * Each command states `while_running`: false when it changes the project (history, scene files,
 * the open scene), which is refused while the game plays or is paused.
 */
namespace command {

/** @brief Selects an instance, or clears the selection with std::nullopt. */
struct Select {
    static constexpr bool while_running = true;
    std::optional<InstanceId> id;
};

struct Undo {
    static constexpr bool while_running = false;
};

struct Redo {
    static constexpr bool while_running = false;
};

/** @brief Writes the open scene to its file. */
struct Save {
    static constexpr bool while_running = true;
};

/** @brief Goes back to the last saved document and clears the history. */
struct DiscardChanges {
    static constexpr bool while_running = false;
};

/** @brief Creates an empty scene file and opens it. */
struct CreateScene {
    static constexpr bool while_running = false;
    std::string name;
};

struct OpenScene {
    static constexpr bool while_running = false;
    std::string name;
};

/** @brief Copies the saved file of a scene; does not open the copy. */
struct DuplicateScene {
    static constexpr bool while_running = false;
    std::string from;
    std::string to;
};

struct RenameScene {
    static constexpr bool while_running = false;
    std::string from;
    std::string to;
};

struct DeleteScene {
    static constexpr bool while_running = false;
    std::string name;
};

/** @brief Makes a scene the one the game starts with (start_scene of game.json). */
struct SetStartScene {
    static constexpr bool while_running = false;
    std::string name;
};

struct SetSnap {
    static constexpr bool while_running = true;
    bool enabled = false;
};

/** @brief Sets the grid size in world units; refused unless finite and positive. */
struct SetGridSize {
    static constexpr bool while_running = true;
    float size = 0.F;
};

struct SetOverlay {
    static constexpr bool while_running = true;
    Overlay overlay = Overlay::hitboxes;
    bool visible = false;
};

struct SetTool {
    static constexpr bool while_running = true;
    Tool tool = Tool::select;
};

struct ShowTab {
    static constexpr bool while_running = true;
    BottomTab tab = BottomTab::console;
};

/** @brief Refuses what the user typed in a field, so the console says why; changes nothing. */
struct ReportInvalidInput {
    static constexpr bool while_running = true;
    std::string message;
};

/** @brief Shows only the prefabs whose type contains `text`, ignoring case; empty shows all. */
struct SetPrefabFilter {
    static constexpr bool while_running = true;
    std::string text;
};

/** @brief Plays from edit mode or resumes from pause; refused while playing. */
struct Play {
    static constexpr bool while_running = true;
};

/** @brief Opens a script file with the program the system uses for it. */
struct OpenScript {
    static constexpr bool while_running = true;
    std::filesystem::path file;
};

/** @brief Pauses a playing game; refused otherwise. */
struct Pause {
    static constexpr bool while_running = true;
};

/** @brief Goes back to edit mode; refused in edit mode. */
struct Stop {
    static constexpr bool while_running = true;
};

/** @brief Opens the export dialog with the game's default settings. */
struct OpenExport {
    static constexpr bool while_running = false;
};

/** @brief Changes the settings of the open export dialog and plans the export again. */
struct SetExport {
    static constexpr bool while_running = false;
    ExportSettings settings;
};

/** @brief Closes the export dialog without exporting. */
struct CloseExport {
    static constexpr bool while_running = true;
};

/** @brief Runs the planned export and closes the dialog; refused while the plan has problems. */
struct RunExport {
    static constexpr bool while_running = false;
};

} // namespace command

using Command = std::variant<command::Select, command::Undo, command::Redo, command::Save,
                             command::DiscardChanges, command::CreateScene, command::OpenScene,
                             command::DuplicateScene, command::RenameScene, command::DeleteScene,
                             command::SetStartScene, command::SetSnap, command::SetGridSize,
                             command::SetOverlay, command::SetTool, command::ShowTab,
                             command::ReportInvalidInput, command::SetPrefabFilter,
                             command::OpenScript, command::Play, command::Pause, command::Stop,
                             command::OpenExport, command::SetExport, command::CloseExport,
                             command::RunExport>;

/** @brief What the UI emits; applied after the frame by Editor::handle(). */
using Action = std::variant<Edit, Command>;

} // namespace editor
