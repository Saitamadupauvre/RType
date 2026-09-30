#include <gtest/gtest.h>

#include "engine/core/ComponentTypeId.hpp"

using engine::core::component_type_id;

namespace {

struct Alpha {};
struct Beta {};

} // namespace

TEST(ComponentTypeId, IsStableForTheSameType) {
    EXPECT_EQ(component_type_id<Alpha>(), component_type_id<Alpha>());
}

TEST(ComponentTypeId, DiffersBetweenTypes) {
    EXPECT_NE(component_type_id<Alpha>(), component_type_id<Beta>());
}

TEST(ComponentTypeId, MatchesLookupByName) {
    EXPECT_EQ(component_type_id<Alpha>(), component_type_id(typeid(Alpha).name()));
}
