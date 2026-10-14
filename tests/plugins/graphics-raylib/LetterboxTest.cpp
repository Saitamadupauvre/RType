#include <gtest/gtest.h>

#include "Letterbox.hpp"

using engine::graphics::Rect;
using engine::graphics::Vec2;

namespace {

constexpr Vec2 virtual_size{.x = 1920.F, .y = 1080.F};

} // namespace

TEST(Letterbox, SameAspectRatioFillsTheWindow) {
    EXPECT_EQ(letterbox({.x = 1920.F, .y = 1080.F}, virtual_size),
              (Rect{.x = 0.F, .y = 0.F, .width = 1920.F, .height = 1080.F}));
}

TEST(Letterbox, SmallerWindowScalesDown) {
    EXPECT_EQ(letterbox({.x = 960.F, .y = 540.F}, virtual_size),
              (Rect{.x = 0.F, .y = 0.F, .width = 960.F, .height = 540.F}));
}

TEST(Letterbox, WiderWindowAddsBarsLeftAndRight) {
    EXPECT_EQ(letterbox({.x = 3840.F, .y = 1080.F}, virtual_size),
              (Rect{.x = 960.F, .y = 0.F, .width = 1920.F, .height = 1080.F}));
}

TEST(Letterbox, TallerWindowAddsBarsTopAndBottom) {
    EXPECT_EQ(letterbox({.x = 1920.F, .y = 2160.F}, virtual_size),
              (Rect{.x = 0.F, .y = 540.F, .width = 1920.F, .height = 1080.F}));
}

TEST(Letterbox, MinimizedWindowGivesEmptyViewport) {
    EXPECT_EQ(letterbox({.x = 0.F, .y = 0.F}, virtual_size),
              (Rect{.x = 0.F, .y = 0.F, .width = 0.F, .height = 0.F}));
}

TEST(WindowToVirtual, SameSizeWindowKeepsThePoint) {
    EXPECT_EQ(window_to_virtual({.x = 100.F, .y = 200.F}, virtual_size, virtual_size),
              (Vec2{.x = 100.F, .y = 200.F}));
}

TEST(WindowToVirtual, SmallerWindowScalesThePointUp) {
    EXPECT_EQ(window_to_virtual({.x = 480.F, .y = 270.F}, {.x = 960.F, .y = 540.F}, virtual_size),
              (Vec2{.x = 960.F, .y = 540.F}));
}

TEST(WindowToVirtual, RemovesTheLetterboxOffset) {
    EXPECT_EQ(window_to_virtual({.x = 960.F, .y = 0.F}, {.x = 3840.F, .y = 1080.F}, virtual_size),
              (Vec2{.x = 0.F, .y = 0.F}));
}

TEST(WindowToVirtual, PointOnABarIsOutsideTheVirtualArea) {
    EXPECT_EQ(window_to_virtual({.x = 0.F, .y = 0.F}, {.x = 3840.F, .y = 1080.F}, virtual_size),
              (Vec2{.x = -960.F, .y = 0.F}));
}

TEST(WindowToVirtual, MinimizedWindowGivesOrigin) {
    EXPECT_EQ(window_to_virtual({.x = 10.F, .y = 10.F}, {.x = 0.F, .y = 0.F}, virtual_size),
              (Vec2{}));
}
