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

TEST(WindowToVirtual, SameSizeWindowKeepsThePoint) {
    EXPECT_EQ(window_to_virtual({100.F, 200.F}, virtual_size, virtual_size), (Vec2{100.F, 200.F}));
}

TEST(WindowToVirtual, SmallerWindowScalesThePointUp) {
    EXPECT_EQ(window_to_virtual({480.F, 270.F}, {960.F, 540.F}, virtual_size),
              (Vec2{960.F, 540.F}));
}

TEST(WindowToVirtual, RemovesTheLetterboxOffset) {
    EXPECT_EQ(window_to_virtual({960.F, 0.F}, {3840.F, 1080.F}, virtual_size), (Vec2{0.F, 0.F}));
}

TEST(WindowToVirtual, PointOnABarIsOutsideTheVirtualArea) {
    EXPECT_EQ(window_to_virtual({0.F, 0.F}, {3840.F, 1080.F}, virtual_size), (Vec2{-960.F, 0.F}));
}

TEST(WindowToVirtual, MinimizedWindowGivesOrigin) {
    EXPECT_EQ(window_to_virtual({10.F, 10.F}, {0.F, 0.F}, virtual_size), (Vec2{}));
}
