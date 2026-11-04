#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../../graphics/fixtures/NullRenderer.hpp"
#include "assets/ShaderLibrary.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/graphics/Types.hpp"

using engine::app::shader_files;
using engine::app::ShaderFiles;
using engine::app::ShaderLibrary;
using engine::core::log::Level;
using engine::core::log::MemorySink;
using engine::core::log::ScopedSink;
using engine::graphics::ShaderId;

namespace {

const std::filesystem::path assets = std::filesystem::path(RTYPE_FIXTURES_DIR) / "app";

class ShaderRenderer final : public fixture::NullRenderer {
public:
    [[nodiscard]] ShaderId load_shader(const std::string& vertex,
                                       const std::string& fragment) override {
        loads.emplace_back(vertex, fragment);
        if (!std::filesystem::exists(fragment)) {
            return {};
        }
        return ShaderId{next++};
    }
    void unload_shader(ShaderId shader) override { unloaded.push_back(shader.value); }

    std::vector<std::pair<std::string, std::string>> loads;
    std::vector<std::uint32_t> unloaded;
    std::uint32_t next{1};
};

std::size_t warnings_in(const MemorySink& sink) {
    std::size_t count = 0;
    for (const auto& record : sink.records()) {
        count += record.level == Level::Warn ? 1 : 0;
    }
    return count;
}

} // namespace

TEST(ShaderFiles, PairsTheVertexShaderWhenItExists) {
    const ShaderFiles files = shader_files(assets / "shaders/wave");

    EXPECT_EQ(files.vertex, assets / "shaders/wave.vs");
    EXPECT_EQ(files.fragment, assets / "shaders/wave.fs");
}

TEST(ShaderFiles, LeavesTheVertexShaderEmptyWithoutAVsFile) {
    const ShaderFiles files = shader_files(assets / "shaders/tint");

    EXPECT_TRUE(files.vertex.empty());
    EXPECT_EQ(files.fragment, assets / "shaders/tint.fs");
}

TEST(ShaderFiles, AlwaysNamesTheFragmentShader) {
    const ShaderFiles files = shader_files(assets / "shaders/missing");

    EXPECT_TRUE(files.vertex.empty());
    EXPECT_EQ(files.fragment, assets / "shaders/missing.fs");
}

TEST(ShaderLibrary, LoadsEachShaderOnceByName) {
    ShaderRenderer renderer;
    ShaderLibrary shaders(renderer, assets);

    const ShaderId first = shaders.get("shaders/wave");
    const ShaderId second = shaders.get("shaders/wave");

    EXPECT_NE(first, ShaderId{});
    EXPECT_EQ(first, second);
    EXPECT_EQ(renderer.loads.size(), 1U);
}

TEST(ShaderLibrary, GivesDistinctNamesDistinctShaders) {
    ShaderRenderer renderer;
    ShaderLibrary shaders(renderer, assets);

    EXPECT_NE(shaders.get("shaders/wave"), shaders.get("shaders/tint"));
}

TEST(ShaderLibrary, PassesBothFilesInsideTheAssetsFolder) {
    ShaderRenderer renderer;
    ShaderLibrary shaders(renderer, assets);

    static_cast<void>(shaders.get("shaders/wave"));
    static_cast<void>(shaders.get("shaders/tint"));

    ASSERT_EQ(renderer.loads.size(), 2U);
    EXPECT_EQ(renderer.loads[0].first, (assets / "shaders/wave.vs").string());
    EXPECT_EQ(renderer.loads[0].second, (assets / "shaders/wave.fs").string());
    EXPECT_EQ(renderer.loads[1].first, "");
    EXPECT_EQ(renderer.loads[1].second, (assets / "shaders/tint.fs").string());
}

TEST(ShaderLibrary, ReportsAMissingShaderOnceAndNeverRetriesIt) {
    ShaderRenderer renderer;
    ShaderLibrary shaders(renderer, assets);
    auto sink = std::make_shared<MemorySink>();
    const ScopedSink scope(sink, Level::Trace);

    EXPECT_EQ(shaders.get("shaders/missing"), ShaderId{});
    EXPECT_EQ(shaders.get("shaders/missing"), ShaderId{});

    EXPECT_EQ(renderer.loads.size(), 1U);
    EXPECT_EQ(warnings_in(*sink), 1U);
}

TEST(ShaderLibrary, RejectsAnInvalidNameWithoutLoadingIt) {
    ShaderRenderer renderer;
    ShaderLibrary shaders(renderer, assets);
    auto sink = std::make_shared<MemorySink>();
    const ScopedSink scope(sink, Level::Trace);

    EXPECT_EQ(shaders.get("../shaders/wave"), ShaderId{});
    EXPECT_EQ(shaders.get("/etc/wave"), ShaderId{});

    EXPECT_TRUE(renderer.loads.empty());
    EXPECT_EQ(warnings_in(*sink), 2U);
}

TEST(ShaderLibrary, PreloadTellsWhetherTheShaderLoaded) {
    ShaderRenderer renderer;
    ShaderLibrary shaders(renderer, assets);

    EXPECT_TRUE(shaders.preload("shaders/wave"));
    EXPECT_FALSE(shaders.preload("shaders/missing"));
    EXPECT_EQ(renderer.loads.size(), 2U);
}

TEST(ShaderLibrary, UnloadsEveryLoadedShaderOnClear) {
    ShaderRenderer renderer;
    ShaderLibrary shaders(renderer, assets);
    const ShaderId wave = shaders.get("shaders/wave");
    const ShaderId tint = shaders.get("shaders/tint");
    static_cast<void>(shaders.get("shaders/missing"));

    shaders.clear();

    std::vector<std::uint32_t> unloaded = renderer.unloaded;
    std::ranges::sort(unloaded);
    EXPECT_EQ(unloaded, (std::vector<std::uint32_t>{wave.value, tint.value}));
}

TEST(ShaderLibrary, ReloadsAShaderAfterClear) {
    ShaderRenderer renderer;
    ShaderLibrary shaders(renderer, assets);
    static_cast<void>(shaders.get("shaders/wave"));

    shaders.clear();
    static_cast<void>(shaders.get("shaders/wave"));

    EXPECT_EQ(renderer.loads.size(), 2U);
}

TEST(ShaderLibrary, UnloadsEveryLoadedShaderWhenDestroyed) {
    ShaderRenderer renderer;
    std::optional<ShaderLibrary> shaders(std::in_place, renderer, assets);
    const ShaderId wave = shaders->get("shaders/wave");

    shaders.reset();

    EXPECT_EQ(renderer.unloaded, (std::vector<std::uint32_t>{wave.value}));
}
