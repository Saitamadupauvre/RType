#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "../script/scene/TempFolder.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/editor/EditorSession.hpp"

class EditorFixture : public ::testing::Test {
protected:
    std::shared_ptr<engine::core::log::MemorySink> _sink =
        std::make_shared<engine::core::log::MemorySink>();
    engine::core::log::ScopedSink _scope{_sink, engine::core::log::Level::Trace};
    TempFolder _game;
    std::optional<engine::editor::EditorSession> _session;

    void SetUp() override {
        std::filesystem::copy(std::filesystem::path(RTYPE_FIXTURES_DIR) / "editor" / "game",
                              _game.path(), std::filesystem::copy_options::recursive);
        _session = engine::editor::EditorSession::open(_game.path());
        ASSERT_TRUE(_session.has_value());
    }

    [[nodiscard]] engine::editor::EditorSession& session() { return _session.value(); }

    [[nodiscard]] bool logged(const std::string& part) const {
        return std::ranges::any_of(_sink->records(), [&](const auto& record) {
            return record.message.find(part) != std::string::npos;
        });
    }

    [[nodiscard]] bool scene_on_disk(const std::string& name) const {
        return std::filesystem::is_regular_file(_game.path() / "scenes" / (name + ".json"));
    }
};
