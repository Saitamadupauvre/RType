#include <gtest/gtest.h>

#include "engine/script/Module.hpp"

TEST(Module, NameMatchesLibrary) { EXPECT_EQ(engine::script::module_name(), "engine-script"); }
