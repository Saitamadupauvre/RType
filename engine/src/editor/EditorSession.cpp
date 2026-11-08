#include "engine/editor/EditorSession.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <format>
#include <span>
#include <system_error>
#include <utility>

#include "SessionState.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/script/EditorApi.hpp"
#include "engine/script/GameFolder.hpp"

namespace engine::editor {

namespace {

namespace fs = std::filesystem;
namespace log = engine::core::log;

constexpr std::string_view position_key = "Position.pos";

CommandResult unknown_instance(InstanceId id) {
    return CommandResult::refused(std::format("no instance with id {}", id.value));
}

std::vector<script::Instance> instances_of(const SceneDocument& document) {
    std::vector<script::Instance> instances;
    instances.reserve(document.instances.size());
    for (const DocumentInstance& entry : document.instances) {
        instances.push_back(entry.instance);
    }
    return instances;
}

std::optional<std::string> override_problem(script::ScriptRuntime& runtime,
                                            const script::Instance& instance, std::string_view key,
                                            const core::FieldValue& value) {
    script::Instance probe = instance;
    probe.overrides = {{.field = std::string(key), .value = value}};
    core::EntityManager scratch;
    const script::Preview preview =
        runtime.preview_instances(scratch, std::span<const script::Instance>(&probe, 1));
    for (const script::InstanceIssue& issue : preview.issues) {
        if (issue.field.empty() || issue.field == key) {
            return issue.message;
        }
    }
    return std::nullopt;
}

void write_override(std::vector<script::FieldOverride>& overrides, std::string_view key,
                    core::FieldValue value, const std::optional<core::FieldValue>& fallback) {
    std::erase_if(overrides,
                  [key](const script::FieldOverride& entry) { return entry.field == key; });
    if (fallback != value) {
        overrides.push_back({.field = std::string(key), .value = std::move(value)});
        std::ranges::sort(overrides, {}, &script::FieldOverride::field);
    }
}

} // namespace

EditorSession::Impl::Impl(fs::path folder)
    : game(std::move(folder)),
      runtime(script::ScriptRuntime::builder().with<script::EditorApi>().build()) {}

void EditorSession::Impl::rebuild() {
    world = std::make_unique<core::EntityManager>();
    const std::vector<script::Instance> instances = instances_of(document);
    script::Preview preview = runtime.preview_instances(*world, instances);
    entities = std::move(preview.entities);
    entities.resize(document.instances.size());
    problems.clear();
    for (script::InstanceIssue& issue : preview.issues) {
        problems.push_back({.instance = document.instances.at(issue.instance).id,
                            .field = std::move(issue.field),
                            .message = std::move(issue.message)});
    }
}

void EditorSession::Impl::forget_missing_selection() noexcept {
    if (selection && document.find(*selection) == nullptr) {
        selection.reset();
    }
}

void EditorSession::Impl::show(std::optional<std::string> name, SceneDocument opened) {
    open_scene = std::move(name);
    document = std::move(opened);
    saved = document;
    history.clear();
    selection.reset();
    rebuild();
}

void EditorSession::Impl::close() { show(std::nullopt, SceneDocument{}); }

CommandResult EditorSession::Impl::commit(SceneDocument next) {
    if (next == document) {
        return CommandResult::done();
    }
    history.record(std::exchange(document, std::move(next)));
    forget_missing_selection();
    rebuild();
    return CommandResult::done();
}

std::optional<EditorSession> EditorSession::open(const fs::path& game) noexcept {
    try {
        std::error_code error;
        if (!fs::is_directory(game, error)) {
            log::error("editor: game folder {} does not exist", game.generic_string());
            return std::nullopt;
        }
        auto impl = std::make_unique<Impl>(game);
        impl->scripts_loaded = impl->runtime.load_declarations(script::scripts_folder(game));
        const auto project = script::read_project(game);
        impl->start_scene = project ? project->start_scene : std::nullopt;
        impl->rebuild();
        return EditorSession(std::move(impl));
    } catch (const std::exception& exception) {
        log::error("editor: cannot open {}: {}", game.generic_string(), exception.what());
    } catch (...) {
        log::error("editor: cannot open {}: unknown error", game.generic_string());
    }
    return std::nullopt;
}

EditorSession::EditorSession(std::unique_ptr<Impl> impl) : _impl(std::move(impl)) {}

EditorSession::~EditorSession() = default;

EditorSession::EditorSession(EditorSession&&) noexcept = default;

EditorSession& EditorSession::operator=(EditorSession&&) noexcept = default;

const fs::path& EditorSession::game() const noexcept { return _impl->game; }

bool EditorSession::scripts_loaded() const noexcept { return _impl->scripts_loaded; }

std::vector<std::string> EditorSession::prefabs() const {
    std::vector<std::string> types = _impl->runtime.entity_types();
    std::ranges::sort(types);
    return types;
}

std::vector<std::string> EditorSession::scenes() const { return script::list_scenes(_impl->game); }

const std::optional<std::string>& EditorSession::start_scene() const noexcept {
    return _impl->start_scene;
}

const script::ScriptRuntime& EditorSession::runtime() const noexcept { return _impl->runtime; }

std::optional<core::FieldValue> EditorSession::default_value(std::string_view type,
                                                             std::string_view key) {
    return _impl->runtime.default_value(type, key);
}

const std::optional<std::string>& EditorSession::open_scene_name() const noexcept {
    return _impl->open_scene;
}

const SceneDocument& EditorSession::document() const noexcept { return _impl->document; }

bool EditorSession::unsaved() const noexcept { return _impl->unsaved(); }

const std::vector<Problem>& EditorSession::problems() const noexcept { return _impl->problems; }

const core::EntityManager& EditorSession::world() const noexcept { return *_impl->world; }

std::optional<core::Entity> EditorSession::entity_of(InstanceId id) const {
    const auto& instances = _impl->document.instances;
    const auto found = std::ranges::find(instances, id, &DocumentInstance::id);
    if (found == instances.end()) {
        return std::nullopt;
    }
    return _impl->entities.at(static_cast<std::size_t>(found - instances.begin()));
}

std::optional<InstanceId> EditorSession::instance_at(core::Entity entity) const {
    const auto& entities = _impl->entities;
    const auto found = std::ranges::find(entities, std::optional(entity));
    if (found == entities.end()) {
        return std::nullopt;
    }
    return _impl->document.instances.at(static_cast<std::size_t>(found - entities.begin())).id;
}

CommandResult EditorSession::place(std::string_view type, core::Vec2 position) {
    if (!_impl->open_scene) {
        return CommandResult::refused("no scene is open");
    }
    const auto declaration = _impl->runtime.find_declaration(type);
    if (!declaration || declaration->kind != script::DeclarationKind::Entity) {
        return CommandResult::refused(std::format("'{}' is not a declared entity", type));
    }
    if (!std::isfinite(position.x) || !std::isfinite(position.y)) {
        return CommandResult::refused("the position must be finite");
    }
    SceneDocument next = _impl->document;
    const InstanceId id = _impl->new_id();
    next.instances.push_back(
        {.id = id,
         .instance = {
             .name = "", .type = std::string(type), .position = position, .overrides = {}}});
    CommandResult result = _impl->commit(std::move(next));
    _impl->selection = id;
    return result;
}

CommandResult EditorSession::move(InstanceId id, core::Vec2 position) {
    if (!std::isfinite(position.x) || !std::isfinite(position.y)) {
        return CommandResult::refused("the position must be finite");
    }
    SceneDocument next = _impl->document;
    DocumentInstance* entry = next.find(id);
    if (entry == nullptr) {
        return unknown_instance(id);
    }
    entry->instance.position = position;
    return _impl->commit(std::move(next));
}

CommandResult EditorSession::remove(InstanceId id) {
    SceneDocument next = _impl->document;
    if (std::erase_if(next.instances,
                      [id](const DocumentInstance& entry) { return entry.id == id; }) == 0) {
        return unknown_instance(id);
    }
    return _impl->commit(std::move(next));
}

CommandResult EditorSession::rename(InstanceId id, std::string name) {
    SceneDocument next = _impl->document;
    DocumentInstance* entry = next.find(id);
    if (entry == nullptr) {
        return unknown_instance(id);
    }
    entry->instance.name = std::move(name);
    return _impl->commit(std::move(next));
}

CommandResult EditorSession::set_field(InstanceId id, std::string_view key,
                                       core::FieldValue value) {
    const DocumentInstance* current = _impl->document.find(id);
    if (current == nullptr) {
        return unknown_instance(id);
    }
    if (key == position_key) {
        return CommandResult::refused("the position is changed with move");
    }
    if (auto problem = override_problem(_impl->runtime, current->instance, key, value)) {
        return CommandResult::refused(std::move(*problem));
    }
    const auto fallback = _impl->runtime.default_value(current->instance.type, key);
    SceneDocument next = _impl->document;
    write_override(next.find(id)->instance.overrides, key, std::move(value), fallback);
    return _impl->commit(std::move(next));
}

CommandResult EditorSession::reset_field(InstanceId id, std::string_view key) {
    SceneDocument next = _impl->document;
    DocumentInstance* entry = next.find(id);
    if (entry == nullptr) {
        return unknown_instance(id);
    }
    std::erase_if(entry->instance.overrides,
                  [key](const script::FieldOverride& item) { return item.field == key; });
    return _impl->commit(std::move(next));
}

CommandResult EditorSession::reset_all(InstanceId id) {
    SceneDocument next = _impl->document;
    DocumentInstance* entry = next.find(id);
    if (entry == nullptr) {
        return unknown_instance(id);
    }
    entry->instance.overrides.clear();
    return _impl->commit(std::move(next));
}

CommandResult EditorSession::change_type(InstanceId id, std::string_view type) {
    const DocumentInstance* current = _impl->document.find(id);
    if (current == nullptr) {
        return unknown_instance(id);
    }
    const auto declaration = _impl->runtime.find_declaration(type);
    if (!declaration || declaration->kind != script::DeclarationKind::Entity) {
        return CommandResult::refused(std::format("'{}' is not a declared entity", type));
    }
    if (current->instance.type == type) {
        return CommandResult::done();
    }
    SceneDocument next = _impl->document;
    script::Instance& instance = next.find(id)->instance;
    instance.type = std::string(type);
    std::vector<script::FieldOverride> kept;
    std::vector<std::string> dropped;
    for (script::FieldOverride& item : instance.overrides) {
        if (override_problem(_impl->runtime, instance, item.field, item.value)) {
            dropped.push_back(item.field);
        } else {
            kept.push_back(std::move(item));
        }
    }
    instance.overrides = std::move(kept);
    CommandResult result = _impl->commit(std::move(next));
    if (result.ok && !dropped.empty()) {
        std::string names;
        for (const std::string& name : dropped) {
            names += (names.empty() ? "" : ", ") + name;
        }
        return CommandResult::done(std::format("dropped what '{}' does not have: {}", type, names));
    }
    return result;
}

CommandResult EditorSession::undo() {
    auto previous = _impl->history.undo(_impl->document);
    if (!previous) {
        return CommandResult::refused("nothing to undo");
    }
    _impl->document = std::move(*previous);
    _impl->forget_missing_selection();
    _impl->rebuild();
    return CommandResult::done();
}

CommandResult EditorSession::redo() {
    auto next = _impl->history.redo(_impl->document);
    if (!next) {
        return CommandResult::refused("nothing to redo");
    }
    _impl->document = std::move(*next);
    _impl->forget_missing_selection();
    _impl->rebuild();
    return CommandResult::done();
}

bool EditorSession::can_undo() const noexcept { return _impl->history.can_undo(); }

bool EditorSession::can_redo() const noexcept { return _impl->history.can_redo(); }

CommandResult EditorSession::select(std::optional<InstanceId> id) {
    if (id && _impl->document.find(*id) == nullptr) {
        return unknown_instance(*id);
    }
    _impl->selection = id;
    return CommandResult::done();
}

std::optional<InstanceId> EditorSession::selection() const noexcept { return _impl->selection; }

} // namespace engine::editor
