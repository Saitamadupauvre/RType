#include <gtest/gtest.h>

#include "engine/graphics/Types.hpp"

using namespace engine::graphics;

TEST(GraphicsTypes, ColorIsOpaqueByDefault) { EXPECT_EQ(Color{}.a, 255); }

TEST(GraphicsTypes, DefaultTextureIdRefersToNoTexture) {
    EXPECT_EQ(TextureId{}.value, 0U);
    EXPECT_NE(TextureId{}, TextureId{.value = 1});
}

TEST(GraphicsTypes, DefaultRenderTargetIdRefersToNoTarget) {
    EXPECT_EQ(RenderTargetId{}.value, 0U);
    EXPECT_NE(RenderTargetId{}, RenderTargetId{.value = 1});
}

TEST(GraphicsTypes, DefaultShaderIdRefersToNoShader) {
    EXPECT_EQ(ShaderId{}.value, 0U);
    EXPECT_NE(ShaderId{}, ShaderId{.value = 1});
}

TEST(GraphicsTypes, ValueTypesCompareFieldByField) {
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

TEST(GraphicsTypes, DefaultSpriteDrawIsUprightFromTopLeft) {
    const SpriteDraw sprite;

    EXPECT_EQ(sprite.rotation, 0.F);
    EXPECT_EQ(sprite.origin, (engine::core::Vec2{0.F, 0.F}));
}

TEST(GraphicsTypes, SpriteDrawMayLeaveOutTheOriginWhenDesignated) {
    const SpriteDraw sprite{.texture = TextureId{.value = 1},
                            .source = {.width = 32, .height = 32},
                            .dest = {.x = 10, .y = 20, .width = 64, .height = 64}};

    EXPECT_EQ(sprite.origin, (engine::core::Vec2{0.F, 0.F}));
    EXPECT_EQ(sprite.rotation, 0.F);
}
