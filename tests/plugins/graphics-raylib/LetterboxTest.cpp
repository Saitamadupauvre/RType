#include <gtest/gtest.h>

#include "Letterbox.hpp"

using engine::core::Vec2;
using engine::graphics::Rect;

namespace {

constexpr Vec2 virtual_size{1920, 1080};

} // namespace

TEST(Letterbox, SameAspectRatioFillsTheWindow) {
    EXPECT_EQ(letterbox({1920, 1080}, virtual_size),
              (Rect{.x = 0.F, .y = 0.F, .width = 1920.F, .height = 1080.F}));
}

TEST(Letterbox, SmallerWindowScalesDown) {
    EXPECT_EQ(letterbox({960, 540}, virtual_size),
              (Rect{.x = 0.F, .y = 0.F, .width = 960.F, .height = 540.F}));
}

TEST(Letterbox, WiderWindowAddsBarsLeftAndRight) {
    EXPECT_EQ(letterbox({3840, 1080}, virtual_size),
              (Rect{.x = 960.F, .y = 0.F, .width = 1920.F, .height = 1080.F}));
}

TEST(Letterbox, TallerWindowAddsBarsTopAndBottom) {
    EXPECT_EQ(letterbox({1920, 2160}, virtual_size),
              (Rect{.x = 0.F, .y = 540.F, .width = 1920.F, .height = 1080.F}));
}

TEST(Letterbox, MinimizedWindowGivesEmptyViewport) {
    EXPECT_EQ(letterbox({0, 0}, virtual_size),
              (Rect{.x = 0.F, .y = 0.F, .width = 0.F, .height = 0.F}));
}
