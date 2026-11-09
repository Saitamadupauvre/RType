#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "UniformLocations.hpp"

TEST(UniformLocations, LooksEachNameUpOnce) {
    UniformLocations locations;
    std::vector<std::string> lookups;
    const auto lookup = [&lookups](const std::string& name) {
        lookups.push_back(name);
        return name == "time" ? 3 : 5;
    };

    EXPECT_EQ(locations.find("time", lookup), 3);
    EXPECT_EQ(locations.find("size", lookup), 5);
    EXPECT_EQ(locations.find("time", lookup), 3);
    EXPECT_EQ(locations.find("size", lookup), 5);

    EXPECT_EQ(lookups, (std::vector<std::string>{"time", "size"}));
}

TEST(UniformLocations, RemembersMissingUniforms) {
    UniformLocations locations;
    int lookups = 0;
    const auto lookup = [&lookups](const std::string&) {
        ++lookups;
        return -1;
    };

    EXPECT_EQ(locations.find("unused", lookup), UniformLocations::missing);
    EXPECT_EQ(locations.find("unused", lookup), UniformLocations::missing);

    EXPECT_EQ(lookups, 1);
}

TEST(UniformLocations, TreatsAnyNegativeLocationAsMissing) {
    UniformLocations locations;

    EXPECT_EQ(locations.find("odd", [](const std::string&) { return -7; }),
              UniformLocations::missing);
}

TEST(UniformLocations, AcceptsLocationZero) {
    UniformLocations locations;

    EXPECT_EQ(locations.find("first", [](const std::string&) { return 0; }), 0);
}
