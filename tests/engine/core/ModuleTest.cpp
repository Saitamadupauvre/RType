#include <gtest/gtest.h>

#include "engine/core/Module.hpp"

TEST(Module, NameMatchesLibrary) { EXPECT_EQ(engine::core::module_name(), "engine-core"); }
