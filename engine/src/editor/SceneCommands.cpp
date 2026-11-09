#include <exception>
#include <filesystem>
#include <format>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>

#include "SessionState.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/editor/EditorSession.hpp"
#include "engine/script/GameFolder.hpp"
#include "engine/script/Scene.hpp"

namespace engine::editor {

namespace {

namespace fs = std::filesystem;
namespace log = engine::core::log;

CommandResult unknown_scene(std::string_view name) {
    return CommandResult::refused(std::format("there is no scene '{}'", name));
}

CommandResult taken_scene(std::string_view name) {
    return CommandResult::refused(std::format("scene '{}' already exists", name));
}

std::string skipped_note(std::size_t count) {
    if (count == 0) {
        return {};
    }
    return std::format("{} instance(s) or override(s) were skipped, see the log", count);
}

std::optional<CommandResult> refuse_names(std::string_view from, std::string_view to) {
    if (auto refused = refuse_scene_name(from)) {
        return refused;
    }
    return refuse_scene_name(to);
}

bool move_file(const fs::path& from, const fs::path& to) noexcept {
    std::error_code error;
    fs::rename(from, to, error);
    if (error) {
        log::error("editor: cannot rename {} to {}: {}", from.generic_string(), to.generic_string(),
                   error.message());
    }
    return !error;
}

} // namespace

std::optional<CommandResult> refuse_scene_name(std::string_view name) {
    if (script::is_scene_name(name)) {
        return std::nullopt;
    }
    return CommandResult::refused(std::format("'{}' is not a valid scene name", name));
}

bool EditorSession::Impl::scene_exists(std::string_view name) const {
    std::error_code error;
    return fs::is_regular_file(script::scene_file(game, name), error);
}

std::optional<CommandResult> EditorSession::Impl::refuse_unsaved() const {
    if (!unsaved()) {
        return std::nullopt;
    }
    return CommandResult::refused(std::format(
        "scene '{}' has unsaved changes: save or discard them first", open_scene.value_or("")));
}

CommandResult EditorSession::create_scene(std::string_view name) {
    if (auto refused = refuse_scene_name(name)) {
        return *refused;
    }
    if (_impl->scene_exists(name)) {
        return taken_scene(name);
    }
    if (auto refused = _impl->refuse_unsaved()) {
        return *refused;
    }
    if (!script::write_scene(script::scene_file(_impl->game, name), script::Scene{})) {
        return CommandResult::refused(std::format("cannot write scene '{}', see the log", name));
    }
    _impl->show(std::string(name), SceneDocument{});
    return CommandResult::done();
}

CommandResult EditorSession::open_scene(std::string_view name) {
    if (auto refused = refuse_scene_name(name)) {
        return *refused;
    }
    if (!_impl->scene_exists(name)) {
        return unknown_scene(name);
    }
    if (auto refused = _impl->refuse_unsaved()) {
        return *refused;
    }
    script::SceneLoad load = _impl->runtime.read_scene(script::scene_file(_impl->game, name));
    if (!load.ok()) {
        return CommandResult::refused(std::format("cannot open scene '{}': {}", name, load.error));
    }
    SceneDocument opened{.instances = {},
                         .unknown_instances = std::move(load.scene.unknown_instances)};
    opened.instances.reserve(load.scene.instances.size());
    for (script::Instance& instance : load.scene.instances) {
        opened.instances.push_back({.id = _impl->new_id(), .instance = std::move(instance)});
    }
    _impl->show(std::string(name), std::move(opened));
    return CommandResult::done(skipped_note(load.issues.size()));
}

CommandResult EditorSession::duplicate_scene(std::string_view from, std::string_view to) {
    if (auto refused = refuse_names(from, to)) {
        return *refused;
    }
    if (!_impl->scene_exists(from)) {
        return unknown_scene(from);
    }
    if (_impl->scene_exists(to)) {
        return taken_scene(to);
    }
    std::error_code error;
    fs::copy_file(script::scene_file(_impl->game, from), script::scene_file(_impl->game, to),
                  error);
    if (error) {
        log::error("editor: cannot copy scene '{}' to '{}': {}", from, to, error.message());
        return CommandResult::refused(std::format("cannot copy scene '{}', see the log", from));
    }
    return CommandResult::done();
}

CommandResult EditorSession::rename_scene(std::string_view from, std::string_view to) {
    if (auto refused = refuse_names(from, to)) {
        return *refused;
    }
    if (!_impl->scene_exists(from)) {
        return unknown_scene(from);
    }
    if (_impl->scene_exists(to)) {
        return taken_scene(to);
    }
    const fs::path old_file = script::scene_file(_impl->game, from);
    const fs::path new_file = script::scene_file(_impl->game, to);
    if (!move_file(old_file, new_file)) {
        return CommandResult::refused(std::format("cannot rename scene '{}', see the log", from));
    }
    if (_impl->start_scene == from) {
        if (!script::write_start_scene(_impl->game, std::string(to))) {
            std::ignore = move_file(new_file, old_file);
            return CommandResult::refused("cannot update game.json, see the log");
        }
        _impl->start_scene = std::string(to);
    }
    if (_impl->open_scene == from) {
        _impl->open_scene = std::string(to);
    }
    return CommandResult::done();
}

CommandResult EditorSession::delete_scene(std::string_view name) {
    if (auto refused = refuse_scene_name(name)) {
        return *refused;
    }
    if (!_impl->scene_exists(name)) {
        return unknown_scene(name);
    }
    if (_impl->start_scene == name) {
        return CommandResult::refused(
            std::format("scene '{}' is the start scene: choose another one first", name));
    }
    const bool is_open = _impl->open_scene == name;
    if (is_open) {
        if (auto refused = _impl->refuse_unsaved()) {
            return *refused;
        }
    }
    std::error_code error;
    fs::remove(script::scene_file(_impl->game, name), error);
    if (error) {
        log::error("editor: cannot delete scene '{}': {}", name, error.message());
        return CommandResult::refused(std::format("cannot delete scene '{}', see the log", name));
    }
    if (is_open) {
        _impl->close();
    }
    return CommandResult::done();
}

CommandResult EditorSession::set_start_scene(std::string_view name) {
    if (auto refused = refuse_scene_name(name)) {
        return *refused;
    }
    if (!_impl->scene_exists(name)) {
        return unknown_scene(name);
    }
    if (!script::write_start_scene(_impl->game, std::string(name))) {
        return CommandResult::refused("cannot update game.json, see the log");
    }
    _impl->start_scene = std::string(name);
    return CommandResult::done();
}

CommandResult EditorSession::save() {
    if (!_impl->open_scene) {
        return CommandResult::refused("no scene is open");
    }
    const fs::path file = script::scene_file(_impl->game, *_impl->open_scene);
    if (!script::write_scene(file, _impl->document.to_scene())) {
        return CommandResult::refused(
            std::format("cannot save scene '{}', see the log", *_impl->open_scene));
    }
    _impl->saved = _impl->document;
    return CommandResult::done();
}

CommandResult EditorSession::discard_changes() {
    if (!_impl->open_scene) {
        return CommandResult::refused("no scene is open");
    }
    _impl->document = _impl->saved;
    _impl->history.clear();
    _impl->forget_missing_selection();
    _impl->rebuild();
    return CommandResult::done();
}

} // namespace engine::editor
