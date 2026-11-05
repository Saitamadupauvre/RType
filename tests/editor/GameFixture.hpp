#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string_view>

#include "../engine/script/scene/TempFolder.hpp"
#include "engine/editor/EditorSession.hpp"
#include "engine/editor/SceneDocument.hpp"

class GameFixture : public ::testing::Test {
protected:
    TempFolder _game;

    GameFixture() {
        std::filesystem::copy(std::filesystem::path(RTYPE_FIXTURES_DIR) / "editor" / "game",
                              _game.path(), std::filesystem::copy_options::recursive);
    }

    [[nodiscard]] engine::editor::EditorSession open_level() const {
        auto session = engine::editor::EditorSession::open(_game.path());
        EXPECT_TRUE(session.has_value());
        EXPECT_TRUE(session->open_scene("level1").ok);
        return std::move(*session);
    }

    [[nodiscard]] static engine::editor::InstanceId
    id_of(const engine::editor::EditorSession& session, std::string_view type) {
        const auto& instances = session.document().instances;
        const auto found = std::ranges::find_if(
            instances, [&](const auto& entry) { return entry.instance.type == type; });
        EXPECT_NE(found, instances.end()) << type;
        return found == instances.end() ? engine::editor::InstanceId{} : found->id;
    }

    [[nodiscard]] static const engine::script::Instance&
    instance(const engine::editor::EditorSession& session, engine::editor::InstanceId id) {
        return session.document().find(id)->instance;
    }
};
