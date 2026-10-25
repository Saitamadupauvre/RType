#include <gtest/gtest.h>

#include "engine/network/Module.hpp"

TEST(Module, NameMatchesLibrary) { EXPECT_EQ(engine::network::module_name(), "engine-network"); }
