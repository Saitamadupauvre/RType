#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "AssetPreloader.hpp"

using engine::app::AssetPreloader;

namespace {

AssetPreloader preloader_of(std::vector<std::string>& calls, const std::vector<bool>& results) {
    AssetPreloader preloader;
    for (std::size_t index = 0; index < results.size(); ++index) {
        const bool found = results[index];
        preloader.add([&calls, index, found] {
            calls.push_back("asset " + std::to_string(index));
            return found;
        });
    }
    return preloader;
}

} // namespace

TEST(AssetPreloader, NothingToLoadIsDoneAtOnce) {
    const AssetPreloader preloader;

    EXPECT_TRUE(preloader.done());
    EXPECT_EQ(preloader.loaded(), 0U);
    EXPECT_EQ(preloader.total(), 0U);
}

TEST(AssetPreloader, LoadsAtMostTheGivenCountPerCallInOrder) {
    std::vector<std::string> calls;
    auto preloader = preloader_of(calls, {true, true, true, true, true});

    preloader.load_next(2);

    EXPECT_EQ(calls, (std::vector<std::string>{"asset 0", "asset 1"}));
    EXPECT_EQ(preloader.loaded(), 2U);
    EXPECT_EQ(preloader.total(), 5U);
    EXPECT_FALSE(preloader.done());
}

TEST(AssetPreloader, IsDoneOnceEveryAssetWasLoadedAndLoadsNothingMore) {
    std::vector<std::string> calls;
    auto preloader = preloader_of(calls, {true, true, true});

    preloader.load_next(2);
    preloader.load_next(2);
    preloader.load_next(2);

    EXPECT_TRUE(preloader.done());
    EXPECT_EQ(preloader.loaded(), 3U);
    EXPECT_EQ(calls.size(), 3U);
}

TEST(AssetPreloader, CountsMissingAssetsAndStillCompletes) {
    std::vector<std::string> calls;
    auto preloader = preloader_of(calls, {true, false, true, false});

    preloader.load_next(10);

    EXPECT_TRUE(preloader.done());
    EXPECT_EQ(preloader.loaded(), 4U);
    EXPECT_EQ(preloader.missing(), 2U);
}

TEST(AssetPreloader, LoadingZeroAssetsMakesNoProgress) {
    std::vector<std::string> calls;
    auto preloader = preloader_of(calls, {true});

    preloader.load_next(0);

    EXPECT_TRUE(calls.empty());
    EXPECT_FALSE(preloader.done());
}
