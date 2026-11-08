#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "play/GameLauncher.hpp"

struct FakeGame {
    std::vector<editor::PlayRequest> launches;
    std::vector<std::string> output;
    std::optional<int> exit;
    std::string refusal;
    bool paused = false;
    bool stopped = false;
};

class FakeRun final : public editor::GameRun {
public:
    explicit FakeRun(std::shared_ptr<FakeGame> game) : _game(std::move(game)) {}

    std::vector<std::string> take_output() override { return std::exchange(_game->output, {}); }
    std::optional<int> exit_code() override { return _game->exit; }
    void pause() override { _game->paused = true; }
    void resume() override { _game->paused = false; }
    void stop() override { _game->stopped = true; }

private:
    std::shared_ptr<FakeGame> _game;
};

class FakeLauncher final : public editor::GameLauncher {
public:
    explicit FakeLauncher(std::shared_ptr<FakeGame> game) : _game(std::move(game)) {}

    Launch launch(const editor::PlayRequest& request) override {
        _game->launches.push_back(request);
        if (!_game->refusal.empty()) {
            return {.run = nullptr, .error = _game->refusal};
        }
        return {.run = std::make_unique<FakeRun>(_game), .error = {}};
    }

private:
    std::shared_ptr<FakeGame> _game;
};
