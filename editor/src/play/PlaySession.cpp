#include "play/PlaySession.hpp"

#include <format>
#include <optional>
#include <system_error>
#include <utility>

#include "engine/script/Scene.hpp"

namespace editor {

namespace {

using engine::editor::CommandResult;

constexpr const char* played_scene = "played-scene.json";

} // namespace

PlaySession::PlaySession(std::filesystem::path scratch) : _scratch(std::move(scratch)) {}

PlaySession::~PlaySession() { finish(); }

PlaySession::PlaySession(PlaySession&&) noexcept = default;

PlaySession& PlaySession::operator=(PlaySession&&) noexcept = default;

void PlaySession::attach(std::unique_ptr<GameLauncher> launcher) {
    _launcher = std::move(launcher);
}

CommandResult PlaySession::play(const engine::editor::EditorSession& session, Mode& mode) {
    if (mode == Mode::playing) {
        return CommandResult::refused("The game is already playing.");
    }
    if (mode == Mode::paused && _run) {
        _run->resume();
        mode = Mode::playing;
        return CommandResult::done();
    }
    if (!session.open_scene_name()) {
        return CommandResult::refused("Open a scene to play it.");
    }
    if (!_launcher) {
        return CommandResult::refused("No game runtime to play with.");
    }
    std::error_code error;
    std::filesystem::create_directories(_scratch, error);
    const std::filesystem::path scene = _scratch / played_scene;
    if (error || !engine::script::write_scene(scene, session.document().to_scene())) {
        return CommandResult::refused(std::format("Cannot write {} to play it.", scene.string()));
    }
    GameLauncher::Launch launch = _launcher->launch({.game = session.game(), .scene_file = scene});
    if (!launch.run) {
        return CommandResult::refused(launch.error);
    }
    _run = std::move(launch.run);
    mode = Mode::playing;
    return CommandResult::done();
}

CommandResult PlaySession::pause(Mode& mode) {
    if (mode != Mode::playing || !_run) {
        return CommandResult::refused("Only a playing game can be paused.");
    }
    _run->pause();
    mode = Mode::paused;
    return CommandResult::done();
}

CommandResult PlaySession::stop(Mode& mode) {
    if (mode == Mode::editing) {
        return CommandResult::refused("The game is not running.");
    }
    finish();
    mode = Mode::editing;
    return CommandResult::done();
}

std::vector<ConsoleLine> PlaySession::update(Mode& mode) {
    std::vector<ConsoleLine> lines;
    if (!_run) {
        return lines;
    }
    const std::optional<int> exit = _run->exit_code();
    for (std::string& text : _run->take_output()) {
        lines.push_back({.severity = Severity::info, .text = std::move(text)});
    }
    if (exit) {
        lines.push_back({.severity = *exit == 0 ? Severity::info : Severity::error,
                         .text = std::format("The game exited with code {}.", *exit)});
        finish();
        mode = Mode::editing;
    }
    return lines;
}

void PlaySession::finish() {
    if (_run) {
        _run->stop();
        _run.reset();
    }
    if (!_scratch.empty()) {
        std::error_code error;
        std::filesystem::remove(_scratch / played_scene, error);
    }
}

} // namespace editor
