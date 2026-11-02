#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "engine/core/AssetCache.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"

using engine::core::AssetCache;
using engine::core::is_valid_asset_name;
using engine::core::max_asset_name_length;
namespace log = engine::core::log;

namespace {

struct TestId {
    std::uint32_t value{0};
    bool operator==(const TestId&) const = default;
};

struct FakeBackend {
    std::vector<std::string> loaded;
    std::vector<std::uint32_t> unloaded;
    std::uint32_t next{1};

    TestId load(const std::filesystem::path& path) {
        loaded.push_back(path.generic_string());
        if (path.filename() == "missing.png") {
            return {};
        }
        return TestId{next++};
    }
};

AssetCache<TestId> cache_over(FakeBackend& backend) {
    return {"assets", [&backend](const std::filesystem::path& path) { return backend.load(path); },
            [&backend](TestId id) { backend.unloaded.push_back(id.value); }};
}

std::size_t warnings_in(const log::MemorySink& sink) {
    std::size_t count = 0;
    for (const auto& record : sink.records()) {
        count += record.level == log::Level::Warn ? 1 : 0;
    }
    return count;
}

} // namespace

TEST(AssetCache, LoadsEachNameOnce) {
    FakeBackend backend;
    auto cache = cache_over(backend);

    const TestId first = cache.get("sprites/ship.png");
    const TestId second = cache.get("sprites/ship.png");

    EXPECT_EQ(first, second);
    EXPECT_EQ(backend.loaded.size(), 1U);
}

TEST(AssetCache, GivesDistinctNamesDistinctHandles) {
    FakeBackend backend;
    auto cache = cache_over(backend);

    EXPECT_NE(cache.get("ship.png"), cache.get("bydo.png"));
    EXPECT_EQ(cache.size(), 2U);
}

TEST(AssetCache, ResolvesNamesInsideTheFolder) {
    FakeBackend backend;
    auto cache = cache_over(backend);

    static_cast<void>(cache.get("sprites/ship.png"));

    EXPECT_EQ(backend.loaded, (std::vector<std::string>{"assets/sprites/ship.png"}));
}

TEST(AssetCache, ReturnsThePlaceholderAndNeverRetriesAFailedLoad) {
    FakeBackend backend;
    auto cache = cache_over(backend);
    auto sink = std::make_shared<log::MemorySink>();
    const log::ScopedSink scope(sink, log::Level::Trace);

    EXPECT_EQ(cache.get("missing.png"), TestId{});
    EXPECT_EQ(cache.get("missing.png"), TestId{});

    EXPECT_EQ(backend.loaded.size(), 1U);
    EXPECT_EQ(warnings_in(*sink), 1U);
}

TEST(AssetCache, RejectsAnInvalidNameWithoutLoadingIt) {
    FakeBackend backend;
    auto cache = cache_over(backend);
    auto sink = std::make_shared<log::MemorySink>();
    const log::ScopedSink scope(sink, log::Level::Trace);

    EXPECT_EQ(cache.get("../secret.png"), TestId{});
    EXPECT_EQ(cache.get("../secret.png"), TestId{});

    EXPECT_TRUE(backend.loaded.empty());
    EXPECT_EQ(warnings_in(*sink), 1U);
}

TEST(AssetCache, UnloadsEveryLoadedAssetOnClear) {
    FakeBackend backend;
    auto cache = cache_over(backend);
    const TestId ship = cache.get("ship.png");
    const TestId bydo = cache.get("bydo.png");

    cache.clear();

    EXPECT_EQ(backend.unloaded.size(), 2U);
    EXPECT_NE(std::ranges::find(backend.unloaded, ship.value), backend.unloaded.end());
    EXPECT_NE(std::ranges::find(backend.unloaded, bydo.value), backend.unloaded.end());
    EXPECT_EQ(cache.size(), 0U);
}

TEST(AssetCache, LoadsAgainAfterClear) {
    FakeBackend backend;
    auto cache = cache_over(backend);
    static_cast<void>(cache.get("ship.png"));

    cache.clear();
    static_cast<void>(cache.get("ship.png"));

    EXPECT_EQ(backend.loaded.size(), 2U);
}

TEST(AssetCache, UnloadsEveryLoadedAssetOnDestruction) {
    FakeBackend backend;
    {
        auto cache = cache_over(backend);
        static_cast<void>(cache.get("ship.png"));
        static_cast<void>(cache.get("bydo.png"));
    }

    EXPECT_EQ(backend.unloaded.size(), 2U);
}

TEST(AssetCache, NeverUnloadsThePlaceholder) {
    FakeBackend backend;
    {
        auto cache = cache_over(backend);
        static_cast<void>(cache.get("missing.png"));
        static_cast<void>(cache.get("../secret.png"));
    }

    EXPECT_TRUE(backend.unloaded.empty());
}

TEST(AssetName, AcceptsPlainAndNestedRelativePaths) {
    EXPECT_TRUE(is_valid_asset_name("laser_shoot.wav"));
    EXPECT_TRUE(is_valid_asset_name("bit-space.mp3"));
    EXPECT_TRUE(is_valid_asset_name("sprites/enemies/bydo.png"));
}

TEST(AssetName, RejectsEmptyName) { EXPECT_FALSE(is_valid_asset_name("")); }

TEST(AssetName, RejectsParentAndCurrentSegments) {
    EXPECT_FALSE(is_valid_asset_name(".."));
    EXPECT_FALSE(is_valid_asset_name("../secret.png"));
    EXPECT_FALSE(is_valid_asset_name("sprites/../../secret.png"));
    EXPECT_FALSE(is_valid_asset_name("./ship.png"));
}

TEST(AssetName, RejectsAbsolutePaths) {
    EXPECT_FALSE(is_valid_asset_name("/etc/passwd"));
    EXPECT_FALSE(is_valid_asset_name("C:/ship.png"));
}

TEST(AssetName, RejectsEmptySegments) {
    EXPECT_FALSE(is_valid_asset_name("sprites//ship.png"));
    EXPECT_FALSE(is_valid_asset_name("sprites/"));
}

TEST(AssetName, RejectsHiddenFiles) {
    EXPECT_FALSE(is_valid_asset_name(".hidden"));
    EXPECT_FALSE(is_valid_asset_name("sprites/.hidden"));
}

TEST(AssetName, RejectsBackslashesAndSpaces) {
    EXPECT_FALSE(is_valid_asset_name("sprites\\ship.png"));
    EXPECT_FALSE(is_valid_asset_name("my ship.png"));
}

TEST(AssetName, RejectsTooLongNames) {
    EXPECT_TRUE(is_valid_asset_name(std::string(max_asset_name_length, 'a')));
    EXPECT_FALSE(is_valid_asset_name(std::string(max_asset_name_length + 1, 'a')));
}
