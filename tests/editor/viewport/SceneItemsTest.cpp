#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>
#include <vector>

#include "../GameFixture.hpp"
#include "viewport/SceneItems.hpp"

using editor::SceneItem;
using engine::core::Vec2;
using engine::editor::InstanceId;

namespace {

class SceneItems : public GameFixture {
protected:
    static bool write_crate(const std::filesystem::path& game) {
        std::ofstream(game / "scripts" / "crate.lua") << "entity \"crate\"\n"
                                                         "color = { r = 200, g = 100, b = 50 }\n"
                                                         "scale = { x = 2, y = -1 }\n"
                                                         "z_index = -5\n"
                                                         "rotation = 30\n";
        return true;
    }

    bool _crate_written = write_crate(_game.path());
    engine::editor::EditorSession _session = open_level();

    static std::optional<Vec2> textures(std::string_view path) {
        if (path == "ship.png") {
            return Vec2{32.F, 16.F};
        }
        return std::nullopt;
    }

    [[nodiscard]] std::vector<SceneItem> items() const {
        return editor::scene_items(_session, textures);
    }

    [[nodiscard]] SceneItem item_of(std::string_view type) const {
        const auto all = items();
        const InstanceId id = id_of(_session, type);
        const auto found = std::ranges::find(all, id, &SceneItem::id);
        EXPECT_NE(found, all.end()) << type;
        return found == all.end() ? SceneItem{} : *found;
    }

    InstanceId place_crate(Vec2 position) {
        EXPECT_TRUE(_session.place("crate", position).ok);
        return id_of(_session, "crate");
    }
};

} // namespace

TEST_F(SceneItems, SpriteIsSizedByItsTexture) {
    const SceneItem ship = item_of("ship");

    EXPECT_EQ(ship.top_left, Vec2(1.F, 2.F));
    EXPECT_EQ(ship.size, Vec2(32.F, 16.F));
    EXPECT_EQ(ship.texture, "ship.png");
    EXPECT_TRUE(ship.visible);
    EXPECT_EQ(ship.tint, (engine::core::Rgba{.r = 10, .g = 20, .b = 30, .a = 255}));
}

TEST_F(SceneItems, EntityWithoutASpriteIsAnInvisibleSquare) {
    const SceneItem rock = item_of("rock");

    EXPECT_FALSE(rock.visible);
    EXPECT_EQ(rock.size, Vec2(editor::square_size, editor::square_size));
}

TEST_F(SceneItems, UnknownTextureIsLaidOutAsASquare) {
    const auto all = editor::scene_items(_session, {});
    const auto ship = std::ranges::find(all, id_of(_session, "ship"), &SceneItem::id);

    ASSERT_NE(ship, all.end());
    EXPECT_EQ(ship->size, Vec2(editor::square_size, editor::square_size));
}

TEST_F(SceneItems, ScaleRotationAndLayerComeFromTheEntity) {
    place_crate({100.F, 100.F});
    const SceneItem crate = item_of("crate");

    EXPECT_EQ(crate.size, Vec2(128.F, 64.F));
    EXPECT_EQ(crate.scale, Vec2(2.F, -1.F));
    EXPECT_FLOAT_EQ(crate.rotation, 30.F);
    EXPECT_EQ(crate.z_index, -5);
    EXPECT_TRUE(crate.texture.empty());
}

TEST_F(SceneItems, LowerLayersComeFirstThenSceneOrder) {
    const InstanceId crate = place_crate({100.F, 100.F});
    const auto all = items();

    ASSERT_EQ(all.size(), 3U);
    EXPECT_EQ(all[0].id, crate);
    EXPECT_EQ(all[1].id, id_of(_session, "ship"));
    EXPECT_EQ(all[2].id, id_of(_session, "rock"));
}

TEST_F(SceneItems, HitTestPicksTheTopmostItem) {
    const auto all = items();

    EXPECT_EQ(editor::item_at(all, {6.F, 7.F}), id_of(_session, "rock"));
    EXPECT_EQ(editor::item_at(all, {2.F, 3.F}), id_of(_session, "ship"));
    EXPECT_EQ(editor::item_at(all, {500.F, 500.F}), std::nullopt);
}

TEST_F(SceneItems, HitTestExcludesTheFarEdge) {
    const auto all = items();

    EXPECT_EQ(editor::item_at(all, {5.F + editor::square_size, 10.F}), std::nullopt);
}

TEST(GridLines, ListTheVisibleLines) {
    const Clay_BoundingBox viewport{.x = 0.F, .y = 0.F, .width = 64.F, .height = 32.F};
    const editor::GridLines lines =
        editor::grid_lines(editor::initial_camera(viewport), viewport, 16.F);

    EXPECT_EQ(lines.xs, (std::vector<float>{0.F, 16.F, 32.F, 48.F, 64.F}));
    EXPECT_EQ(lines.ys, (std::vector<float>{0.F, 16.F, 32.F}));
}

TEST(GridLines, NoneWhenTooDenseOnScreen) {
    const Clay_BoundingBox viewport{.x = 0.F, .y = 0.F, .width = 64.F, .height = 32.F};
    const editor::ViewCamera camera{.center = {0.F, 0.F}, .zoom = 0.1F};

    EXPECT_TRUE(editor::grid_lines(camera, viewport, 16.F).xs.empty());
}
