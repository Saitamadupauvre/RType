#include <gtest/gtest.h>

#include <cstdint>

#include "KeyMap.hpp"

using engine::graphics::Key;

namespace {

constexpr int raylib_key_null = 0;
constexpr int raylib_key_a = 65;
constexpr int raylib_key_space = 32;
constexpr int raylib_key_volume_up = 24;

} // namespace

TEST(KeyMap, EveryKeySurvivesARoundTrip) {
    const auto last = static_cast<std::uint8_t>(Key::GraveAccent);
    for (std::uint8_t value = 1; value <= last; ++value) {
        const auto key = static_cast<Key>(value);
        EXPECT_EQ(from_raylib(to_raylib(key)), key) << "key " << static_cast<int>(value);
    }
}

TEST(KeyMap, MapsKnownKeysToRaylibCodes) {
    EXPECT_EQ(to_raylib(Key::A), raylib_key_a);
    EXPECT_EQ(to_raylib(Key::Space), raylib_key_space);
    EXPECT_EQ(from_raylib(raylib_key_a), Key::A);
}

TEST(KeyMap, UnknownKeyHasNoRaylibCode) { EXPECT_EQ(to_raylib(Key::Unknown), raylib_key_null); }

TEST(KeyMap, UnmappedRaylibCodesGiveUnknown) {
    EXPECT_EQ(from_raylib(raylib_key_null), Key::Unknown);
    EXPECT_EQ(from_raylib(raylib_key_volume_up), Key::Unknown);
    EXPECT_EQ(from_raylib(-1), Key::Unknown);
    EXPECT_EQ(from_raylib(9999), Key::Unknown);
}
