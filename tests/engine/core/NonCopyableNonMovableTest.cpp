#include <gtest/gtest.h>

#include <type_traits>

#include "engine/core/NonCopyableNonMovable.hpp"

using engine::core::NonCopyableNonMovable;

namespace {

struct Pinned : NonCopyableNonMovable {
    int value{0};
};

} // namespace

TEST(NonCopyableNonMovable, DerivedTypeCannotBeCopiedOrMoved) {
    static_assert(!std::is_copy_constructible_v<Pinned>);
    static_assert(!std::is_copy_assignable_v<Pinned>);
    static_assert(!std::is_move_constructible_v<Pinned>);
    static_assert(!std::is_move_assignable_v<Pinned>);
    SUCCEED();
}

TEST(NonCopyableNonMovable, DerivedTypeIsDefaultConstructible) {
    static_assert(std::is_default_constructible_v<Pinned>);
    const Pinned pinned;

    EXPECT_EQ(pinned.value, 0);
}

TEST(NonCopyableNonMovable, BaseCannotBeCreatedOnItsOwn) {
    static_assert(!std::is_default_constructible_v<NonCopyableNonMovable>);
    SUCCEED();
}

TEST(NonCopyableNonMovable, AddsNoSize) { static_assert(sizeof(Pinned) == sizeof(int)); }
