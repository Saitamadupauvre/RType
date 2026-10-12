#include <gtest/gtest.h>

#include "engine/graphics/Types.hpp"

using namespace engine::graphics;

TEST(GraphicsTypes, ColorIsOpaqueByDefault) { EXPECT_EQ(Color{}.a, 255); }

TEST(GraphicsTypes, DefaultTextureIdRefersToNoTexture) {
    EXPECT_EQ(TextureId{}.value, 0U);
    EXPECT_NE(TextureId{}, TextureId{.value = 1});
}

TEST(GraphicsTypes, ValueTypesCompareFieldByField) {
    EXPECT_EQ((Vec2{.x = 1.F, .y = 2.F}), (Vec2{.x = 1.F, .y = 2.F}));
    EXPECT_NE((Vec2{.x = 1.F, .y = 2.F}), (Vec2{.x = 2.F, .y = 1.F}));
    EXPECT_EQ((Rect{.x = 0.F, .y = 0.F, .width = 10.F, .height = 5.F}),
              (Rect{.x = 0.F, .y = 0.F, .width = 10.F, .height = 5.F}));
    EXPECT_NE((Color{.r = 255}), (Color{.g = 255}));
}

TEST(GraphicsTypes, DefaultWindowIsWindowed) {
    const WindowConfig config;

    EXPECT_GT(config.width, 0);
    EXPECT_GT(config.height, 0);
    EXPECT_FALSE(config.fullscreen);
}

TEST(GraphicsTypes, UnknownKeyIsTheDefault) { EXPECT_EQ(Key{}, Key::Unknown); }
