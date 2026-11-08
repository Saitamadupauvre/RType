#include <gtest/gtest.h>

#include <vector>

#include "../../engine/script/scene/TempFolder.hpp"
#include "assets/AssetList.hpp"

using editor::asset_field;
using editor::AssetEntry;
using editor::AssetKind;
using editor::kind_label;
using editor::list_assets;

TEST(AssetList, ListsFilesSortedWithTheirKind) {
    const TempFolder game;
    game.write("assets/sprites/ship.png", "");
    game.write("assets/sounds/shot.wav", "");
    game.write("assets/music/theme.mp3", "");
    game.write("assets/shaders/space.fs", "");
    game.write("assets/CREDITS.md", "");
    game.write("assets/Big.PNG", "");

    EXPECT_EQ(list_assets(game.path()),
              (std::vector<AssetEntry>{{.path = "Big.PNG", .kind = AssetKind::sprite},
                                       {.path = "CREDITS.md", .kind = AssetKind::other},
                                       {.path = "music/theme.mp3", .kind = AssetKind::music},
                                       {.path = "shaders/space.fs", .kind = AssetKind::shader},
                                       {.path = "sounds/shot.wav", .kind = AssetKind::sound},
                                       {.path = "sprites/ship.png", .kind = AssetKind::sprite}}));
}

TEST(AssetList, AudioOutsideTheMusicFolderIsASound) {
    const TempFolder game;
    game.write("assets/sfx/music/loop.ogg", "");

    EXPECT_EQ(list_assets(game.path()).front().kind, AssetKind::sound);
}

TEST(AssetList, SkipsHiddenFilesAndFolders) {
    const TempFolder game;
    game.write("assets/.cache/thumb.png", "");
    game.write("assets/.DS_Store", "");
    game.write("assets/ship.png", "");

    EXPECT_EQ(list_assets(game.path()),
              (std::vector<AssetEntry>{{.path = "ship.png", .kind = AssetKind::sprite}}));
}

TEST(AssetList, MissingFolderIsEmpty) {
    const TempFolder game;

    EXPECT_TRUE(list_assets(game.path()).empty());
}

TEST(AssetList, StopsAtTheLimit) {
    const TempFolder game;
    for (std::size_t index = 0; index <= editor::max_assets; ++index) {
        game.write("assets/f" + std::to_string(index) + ".png", "");
    }

    EXPECT_EQ(list_assets(game.path()).size(), editor::max_assets);
}

TEST(AssetList, OnlySpritesAndShadersSetAField) {
    EXPECT_EQ(asset_field(AssetKind::sprite), "Sprite.path");
    EXPECT_EQ(asset_field(AssetKind::shader), "Shader.path");
    EXPECT_EQ(asset_field(AssetKind::sound), std::nullopt);
    EXPECT_EQ(asset_field(AssetKind::music), std::nullopt);
    EXPECT_EQ(asset_field(AssetKind::other), std::nullopt);
}

TEST(AssetList, KindsHaveLabels) {
    EXPECT_EQ(kind_label(AssetKind::sprite), "sprite");
    EXPECT_EQ(kind_label(AssetKind::other), "file");
}
