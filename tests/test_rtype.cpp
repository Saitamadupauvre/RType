#include <gtest/gtest.h>

#include "rtype/rtype.hpp"

TEST(rtype, Greet) { EXPECT_EQ(rtype::greet("world"), "Hello, world!"); }
