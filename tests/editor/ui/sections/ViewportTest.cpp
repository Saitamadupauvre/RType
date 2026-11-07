#include <gtest/gtest.h>

#include <variant>
#include <vector>

#include "../FrameFixture.hpp"
#include "action/Action.hpp"
#include "engine/core/math/Vec2.hpp"

using editor::Tool;
using editor::UiInput;
using engine::core::Vec2;

namespace command = editor::command;
namespace edit = editor::edit;

namespace {

class Viewport : public FrameFixture {
protected:
    void SetUp() override {
        frame();
        frame();
    }

    [[nodiscard]] static Clay_Vector2 screen(Vec2 world) {
        const Clay_BoundingBox box = box_of(CLAY_ID("Viewport"));
        return {.x = box.x + world.x, .y = box.y + world.y};
    }

    void use(Tool tool) { _editor.handle(command::SetTool{.tool = tool}); }

    void select_ship() { _editor.handle(command::Select{.id = id_of(_editor.session(), "ship")}); }

    std::vector<editor::Action> drag(Vec2 from, Vec2 to, bool alt = false) {
        std::vector<editor::Action> actions;
        const auto collect = [&](const UiInput& input) {
            for (auto& action : frame(input).actions) {
                actions.push_back(std::move(action));
            }
        };
        collect({.pointer = screen(from), .primary_down = true, .primary_pressed = true});
        collect({.pointer = screen(to), .primary_down = true, .alt = alt});
        collect({.pointer = screen(to), .primary_down = true, .alt = alt});
        collect({.pointer = screen(to), .alt = alt});
        return actions;
    }

    template <typename Wanted>
    static const Wanted* only_edit(const std::vector<editor::Action>& actions) {
        if (actions.size() != 1) {
            return nullptr;
        }
        const auto* change = std::get_if<editor::Edit>(&actions.front());
        return change ? std::get_if<Wanted>(change) : nullptr;
    }

    editor::ViewportState& state() { return _widgets.viewport(); }

    editor::ViewCamera camera() { return state().camera.value_or(editor::ViewCamera{}); }
};

} // namespace

TEST_F(Viewport, ClickOnAnInstanceSelectsIt) {
    const auto actions =
        frame({.pointer = screen({2.F, 3.F}), .primary_down = true, .primary_pressed = true})
            .actions;

    const auto* select = only<command::Select>(actions);
    ASSERT_NE(select, nullptr);
    EXPECT_EQ(select->id, id_of(_editor.session(), "ship"));
}

TEST_F(Viewport, ClickOnTheTopmostOfOverlappingInstancesSelectsIt) {
    const auto actions =
        frame({.pointer = screen({30.F, 10.F}), .primary_down = true, .primary_pressed = true})
            .actions;

    const auto* select = only<command::Select>(actions);
    ASSERT_NE(select, nullptr);
    EXPECT_EQ(select->id, id_of(_editor.session(), "rock"));
}

TEST_F(Viewport, ClickOnEmptySpaceClearsTheSelection) {
    select_ship();
    const auto actions =
        frame({.pointer = screen({300.F, 300.F}), .primary_down = true, .primary_pressed = true})
            .actions;

    const auto* select = only<command::Select>(actions);
    ASSERT_NE(select, nullptr);
    EXPECT_FALSE(select->id.has_value());
}

TEST_F(Viewport, ClickOnTheSelectionEmitsNothing) {
    select_ship();
    EXPECT_TRUE(
        frame({.pointer = screen({2.F, 3.F}), .primary_down = true, .primary_pressed = true})
            .actions.empty());
}

TEST_F(Viewport, MoveToolDragEmitsOneSnappedMove) {
    use(Tool::move);
    select_ship();
    const auto actions = drag({2.F, 3.F}, {40.F, 30.F});

    const auto* move = only_edit<edit::Move>(actions);
    ASSERT_NE(move, nullptr);
    EXPECT_EQ(move->id, id_of(_editor.session(), "ship"));
    EXPECT_EQ(move->position, Vec2(32.F, 32.F));
}

TEST_F(Viewport, AltDropsWithoutSnapping) {
    use(Tool::move);
    select_ship();
    const auto actions = drag({2.F, 3.F}, {40.F, 30.F}, true);

    const auto* move = only_edit<edit::Move>(actions);
    ASSERT_NE(move, nullptr);
    EXPECT_EQ(move->position, Vec2(39.F, 29.F));
}

TEST_F(Viewport, DragShowsThePreviewBeforeTheRelease) {
    use(Tool::move);
    select_ship();
    frame({.pointer = screen({2.F, 3.F}), .primary_down = true, .primary_pressed = true});
    frame({.pointer = screen({40.F, 30.F}), .primary_down = true});
    const auto actions = frame({.pointer = screen({40.F, 30.F}), .primary_down = true}).actions;

    EXPECT_TRUE(actions.empty());
    const auto& items = state().frame.items;
    const auto ship =
        std::ranges::find(items, id_of(_editor.session(), "ship"), &editor::SceneItem::id);
    ASSERT_NE(ship, items.end());
    EXPECT_EQ(ship->top_left, Vec2(32.F, 32.F));
}

TEST_F(Viewport, ReleasingWhereItStartedEmitsNothing) {
    use(Tool::move);
    select_ship();
    EXPECT_TRUE(drag({2.F, 3.F}, {2.F, 3.F}).empty());
}

TEST_F(Viewport, SelectToolDoesNotMove) {
    select_ship();
    EXPECT_TRUE(drag({2.F, 3.F}, {40.F, 30.F}).empty());
}

TEST_F(Viewport, WheelZoomsWithoutEmitting) {
    const auto actions =
        frame({.pointer = screen({100.F, 100.F}), .wheel = {.x = 0.F, .y = 2.F}}).actions;

    EXPECT_TRUE(actions.empty());
    ASSERT_TRUE(state().camera.has_value());
    EXPECT_GT(camera().zoom, 1.F);
}

TEST_F(Viewport, MiddleDragPans) {
    const Vec2 before = camera().center;
    frame({.pointer = screen({100.F, 100.F}), .middle_down = true});
    frame({.pointer = screen({130.F, 90.F}), .middle_down = true});

    EXPECT_EQ(camera().center, before - Vec2(30.F, -10.F));
}

TEST_F(Viewport, PanToolDragPansWithoutSelecting) {
    use(Tool::pan);
    const Vec2 before = camera().center;
    std::vector<editor::Action> actions = drag({2.F, 3.F}, {22.F, 3.F});

    EXPECT_TRUE(actions.empty());
    EXPECT_EQ(camera().center, before - Vec2(20.F, 0.F));
}

TEST_F(Viewport, FrameHoldsTheSceneAndTheSelection) {
    select_ship();
    frame();

    EXPECT_EQ(state().frame.items.size(), 2U);
    EXPECT_EQ(state().frame.selection, id_of(_editor.session(), "ship"));
    EXPECT_FALSE(state().frame.grid.xs.empty());
}
