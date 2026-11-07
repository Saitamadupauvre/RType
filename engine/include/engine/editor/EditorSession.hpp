#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/core/ecs/Entity.hpp"
#include "engine/core/ecs/EntityManager.hpp"
#include "engine/core/ecs/FieldRegistry.hpp"
#include "engine/core/math/Vec2.hpp"
#include "engine/editor/CommandResult.hpp"
#include "engine/editor/Export.hpp"
#include "engine/editor/SceneDocument.hpp"
#include "engine/script/ScriptRuntime.hpp"

namespace engine::editor {

/** @brief Something the edit world could not build for an instance of the open scene. */
struct Problem {
    InstanceId instance;
    /** Field of the skipped override, empty when the whole instance was skipped. */
    std::string field;
    std::string message;

    bool operator==(const Problem&) const = default;
};

/**
 * @brief Editing model of one game folder: its prefabs, its scenes and the open scene.
 *
 * Holds no window and no UI. A front displays what the getters return and calls the commands;
 * every command returns a CommandResult, and a refused command changes nothing.
 *
 * The **document** (document()) is the source of truth of the open scene. The **edit world**
 * (world()) is rebuilt from it after every change with script::ScriptRuntime::preview_instances(),
 * so no Lua callback ever runs in edit mode: no `on_start`, `on_spawn`, `update`, `on_collide`,
 * `on_destroy` and no system. What must be visible in the editor therefore belongs in the
 * declaration, not in `on_spawn`.
 *
 * **Edits** (place(), move(), remove(), rename(), set_field(), reset_field(), reset_all()) change
 * the document and can be undone. An edit that changes nothing succeeds without being recorded.
 * Up to @ref history_limit edits are kept. The scene is unsaved while the document differs from
 * the last saved or opened one, so undoing back to it clears the flag.
 *
 * **Scene commands** (create_scene(), open_scene(), duplicate_scene(), rename_scene(),
 * delete_scene(), set_start_scene()) act on the files of `<game>/scenes/` and on `game.json`.
 * One scene is open at a time; a command that would drop unsaved changes is refused until they
 * are saved with save() or dropped with discard_changes().
 *
 * @note Not thread safe. Not copyable; movable.
 */
class ENGINE_EDITOR_EXPORT EditorSession {
public:
    /** Number of edits undo() can go back. */
    static constexpr std::size_t history_limit = 100;

    /**
     * @brief Opens a game folder: loads the declarations of its `scripts/` with the `core` and
     * `editor` API sets, without starting the game, and reads `game.json`.
     *
     * No scene is open afterwards.
     *
     * @return std::nullopt, after logging why, when @p game is not a folder. Script errors are
     * logged and do not prevent opening: scripts_loaded() reports them.
     * @note Never throws.
     */
    [[nodiscard]] static std::optional<EditorSession>
    open(const std::filesystem::path& game) noexcept;

    ~EditorSession();
    EditorSession(EditorSession&&) noexcept;
    EditorSession& operator=(EditorSession&&) noexcept;
    EditorSession(const EditorSession&) = delete;
    EditorSession& operator=(const EditorSession&) = delete;

    /** @name Game folder */
    ///@{
    [[nodiscard]] const std::filesystem::path& game() const noexcept;
    /** @return false when a script failed to load; what did load is still usable. */
    [[nodiscard]] bool scripts_loaded() const noexcept;
    /** @return The declared entity types that can be placed, in ascending order. */
    [[nodiscard]] std::vector<std::string> prefabs() const;
    /** @return The scenes of the game folder, see script::list_scenes(). */
    [[nodiscard]] std::vector<std::string> scenes() const;
    /** @return The `start_scene` of `game.json`, std::nullopt when it names none. */
    [[nodiscard]] const std::optional<std::string>& start_scene() const noexcept;
    /** @return The runtime holding the declarations, for script_fields() and the like. */
    [[nodiscard]] const script::ScriptRuntime& runtime() const noexcept;
    /**
     * @return The value an instance of @p type has for @p key when it does not override it,
     * see script::ScriptRuntime::default_value().
     */
    [[nodiscard]] std::optional<core::FieldValue> default_value(std::string_view type,
                                                                std::string_view key);
    ///@}

    /** @name Scene commands */
    ///@{
    /**
     * @brief Writes an empty scene file named @p name and opens it.
     * Refused for an invalid name, an existing scene or unsaved changes.
     */
    CommandResult create_scene(std::string_view name);
    /**
     * @brief Reads scene @p name into the document; history and selection are cleared.
     * Refused for an invalid or unknown name, a file read_scene() rejects, or unsaved changes.
     * Instances and overrides read_scene() skips are logged and counted in the message.
     */
    CommandResult open_scene(std::string_view name);
    /**
     * @brief Copies the saved file of scene @p from to a new scene @p to, without opening it.
     * Refused for an invalid name, an unknown @p from or an existing @p to.
     */
    CommandResult duplicate_scene(std::string_view from, std::string_view to);
    /**
     * @brief Renames scene @p from to @p to, following it in `game.json` and in the open scene.
     * Refused for an invalid name, an unknown @p from or an existing @p to.
     */
    CommandResult rename_scene(std::string_view from, std::string_view to);
    /**
     * @brief Deletes scene @p name; closes it if it is open.
     * Refused for an unknown scene, the start scene, or the open scene with unsaved changes.
     */
    CommandResult delete_scene(std::string_view name);
    /** @brief Makes @p name the `start_scene` of `game.json`. Refused for an unknown scene. */
    CommandResult set_start_scene(std::string_view name);
    /** @brief Writes the document to the open scene's file. Refused when no scene is open. */
    CommandResult save();
    /**
     * @brief Goes back to the last saved or opened document; history is cleared.
     * Refused when no scene is open.
     */
    CommandResult discard_changes();
    ///@}

    /** @name Open scene */
    ///@{
    /** @return The name of the open scene, std::nullopt when none is open. */
    [[nodiscard]] const std::optional<std::string>& open_scene_name() const noexcept;
    [[nodiscard]] const SceneDocument& document() const noexcept;
    [[nodiscard]] bool unsaved() const noexcept;
    /** @return What the edit world could not build, in document order. */
    [[nodiscard]] const std::vector<Problem>& problems() const noexcept;
    /** @return The edit world, rebuilt after every change. */
    [[nodiscard]] const core::EntityManager& world() const noexcept;
    /** @return The edit-world entity of an instance, std::nullopt when it was not built. */
    [[nodiscard]] std::optional<core::Entity> entity_of(InstanceId id) const;
    /** @return The instance an edit-world entity was built for. */
    [[nodiscard]] std::optional<InstanceId> instance_at(core::Entity entity) const;
    ///@}

    /** @name Edits */
    ///@{
    /**
     * @brief Adds an instance of @p type at @p position and selects it.
     * Refused when no scene is open or @p type is not a declared entity.
     */
    CommandResult place(std::string_view type, core::Vec2 position);
    /** @brief Moves an instance. Refused for an unknown id or a non-finite position. */
    CommandResult move(InstanceId id, core::Vec2 position);
    /** @brief Removes an instance and drops it from the selection. Refused for an unknown id. */
    CommandResult remove(InstanceId id);
    /** @brief Sets the label of an instance; an empty name removes it. */
    CommandResult rename(InstanceId id, std::string name);
    /**
     * @brief Overrides field @p key of an instance with @p value.
     *
     * @p key is `"Component.field"` of an editable registry field or `"Script.<name>"` of a
     * script field of the instance's type. A value equal to default_value() removes the override.
     * Refused for an unknown id, `Position.pos` (use move()), an unknown or non-editable key, a
     * value of the wrong kind, or a value the component refuses (a negative hitbox size...).
     */
    CommandResult set_field(InstanceId id, std::string_view key, core::FieldValue value);
    /** @brief Removes the override of @p key, back to the prefab's value. */
    CommandResult reset_field(InstanceId id, std::string_view key);
    /** @brief Removes every override of an instance. */
    CommandResult reset_all(InstanceId id);
    /**
     * @brief Turns an instance into another declared entity, keeping its name, its position and
     * the overrides the new type still accepts. Undoable like any edit.
     * @return Refused for an unknown instance or a type that is not a declared entity; done with
     * a note naming the dropped overrides, if any. The same type changes nothing.
     */
    CommandResult change_type(InstanceId id, std::string_view type);
    /** @brief Goes back one edit. Refused when there is none. */
    CommandResult undo();
    /** @brief Applies again the last undone edit. Refused when there is none. */
    CommandResult redo();
    [[nodiscard]] bool can_undo() const noexcept;
    [[nodiscard]] bool can_redo() const noexcept;
    ///@}

    /** @name Selection (not part of undo) */
    ///@{
    /** @brief Selects an instance, or clears the selection. Refused for an unknown id. */
    CommandResult select(std::optional<InstanceId> id);
    [[nodiscard]] std::optional<InstanceId> selection() const noexcept;
    ///@}

private:
    struct Impl;

    explicit EditorSession(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> _impl;
};

} // namespace engine::editor
