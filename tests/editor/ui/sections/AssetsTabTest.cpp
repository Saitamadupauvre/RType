#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <variant>
#include <vector>

#include "../FrameFixture.hpp"
#include "action/Action.hpp"
#include "engine/core/ecs/FieldRegistry.hpp"
#include "engine/core/math/Vec2.hpp"

using editor::BottomTab;
using engine::core::Vec2;

namespace command = editor::command;
namespace edit = editor::edit;

namespace {

class AssetsTab : public FrameFixture {
protected:
    void SetUp() override {
        _game.write("assets/shaders/glow.fs", "");
        _game.write("assets/sounds/shot.wav", "");
        _game.write("assets/sprites/ship.png", "");
        _editor.handle(command::ShowTab{.tab = BottomTab::assets});
        frame();
        frame();
    }

    static Clay_ElementId row(std::uint32_t index) { return CLAY_IDI("AssetRow", index); }

    [[nodiscard]] static Clay_Vector2 screen(Vec2 world) {
        const Clay_BoundingBox box = box_of(CLAY_ID("Viewport"));
        return {.x = box.x + world.x, .y = box.y + world.y};
    }

    std::vector<editor::Action> drop_on(std::uint32_t index, Vec2 world) {
        click(row(index));
        frame({.pointer = screen(world), .primary_down = true});
        frame({.pointer = screen(world), .primary_down = true});
        return frame({.pointer = screen(world)}).actions;
    }

    static const edit::SetField* set_field(const std::vector<editor::Action>& actions) {
        if (actions.size() != 1) {
            return nullptr;
        }
        const auto* change = std::get_if<editor::Edit>(&actions.front());
        return change ? std::get_if<edit::SetField>(change) : nullptr;
    }

    editor::ViewportState& state() { return _widgets.viewport(); }
};

} // namespace

TEST_F(AssetsTab, ListsTheAssetsWithTheirKind) {
    const auto texts = frame().texts;

    EXPECT_NE(std::ranges::find(texts, "Assets 3"), texts.end());
    EXPECT_NE(std::ranges::find(texts, "sprites/ship.png"), texts.end());
    EXPECT_NE(std::ranges::find(texts, "sounds/shot.wav"), texts.end());
    EXPECT_NE(std::ranges::find(texts, "shader"), texts.end());
}

TEST_F(AssetsTab, DroppingASpriteOnAnInstanceSetsItsSprite) {
    const auto actions = drop_on(2, {2.F, 3.F});

    const auto* change = set_field(actions);
    ASSERT_NE(change, nullptr);
    EXPECT_EQ(change->id, id_of(_editor.session(), "ship"));
    EXPECT_EQ(change->key, "Sprite.path");
    EXPECT_EQ(change->value,
              engine::core::FieldValue{engine::core::AssetName{.path = "sprites/ship.png"}});
    EXPECT_FALSE(state().asset_drag.has_value());
}

TEST_F(AssetsTab, DroppingAShaderSetsTheShader) {
    const auto actions = drop_on(0, {2.F, 3.F});
    const auto* change = set_field(actions);

    ASSERT_NE(change, nullptr);
    EXPECT_EQ(change->key, "Shader.path");
    EXPECT_EQ(change->value,
              engine::core::FieldValue{engine::core::AssetName{.path = "shaders/glow"}});
}

TEST_F(AssetsTab, TheTargetIsOutlinedWhileDragging) {
    click(row(2));
    frame({.pointer = screen({10.F, 10.F}), .primary_down = true});
    frame({.pointer = screen({10.F, 10.F}), .primary_down = true});

    EXPECT_TRUE(state().frame.ghost.has_value());
}

TEST_F(AssetsTab, DroppingOnEmptySpaceChangesNothing) {
    EXPECT_TRUE(drop_on(2, {900.F, 500.F}).empty());
    EXPECT_FALSE(state().asset_drag.has_value());
}

TEST_F(AssetsTab, ReleasingOverTheListCancels) {
    click(row(2));
    const Clay_BoundingBox box = box_of(row(2));

    EXPECT_TRUE(frame({.pointer = {.x = box.x + 2.F, .y = box.y + 2.F}}).actions.empty());
    EXPECT_FALSE(state().asset_drag.has_value());
}

TEST_F(AssetsTab, SoundsCannotBeDropped) {
    click(row(1));

    EXPECT_FALSE(state().asset_drag.has_value());
}

TEST_F(AssetsTab, DoubleClickOpensTheFile) {
    frame();
    const Clay_BoundingBox box = box_of(row(1));
    const auto actions = frame({.pointer = {.x = box.x + 2.F, .y = box.y + 2.F},
                                .primary_down = true,
                                .primary_pressed = true,
                                .click_count = 2})
                             .actions;

    const auto* open = only<command::OpenScript>(actions);
    ASSERT_NE(open, nullptr);
    EXPECT_EQ(open->file, _game.path() / "assets" / "sounds/shot.wav");
}

TEST_F(AssetsTab, ShowsAHintWithoutAssets) {
    std::filesystem::remove_all(_game.path() / "assets");
    _editor.handle(command::ShowTab{.tab = BottomTab::assets});

    EXPECT_TRUE(shows("No files in assets/"));
}
