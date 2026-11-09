#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "../../graphics/fixtures/NullRenderer.hpp"
#include "assets/TextureLibrary.hpp"
#include "engine/graphics/Types.hpp"

using engine::app::TextureLibrary;
using engine::graphics::TextureId;

namespace {

constexpr TextureId ship{.value = 4};

class LoadingRenderer final : public fixture::NullRenderer {
public:
    [[nodiscard]] TextureId load_texture(const std::string& path) override {
        loaded.push_back(path);
        return path.ends_with("missing.png") ? TextureId{} : ship;
    }
    void unload_texture(TextureId texture) override { unloaded.push_back(texture); }

    std::vector<std::string> loaded;
    std::vector<TextureId> unloaded;
};

} // namespace

TEST(TextureLibrary, LoadsATextureFromTheAssetsFolderOnce) {
    LoadingRenderer renderer;
    TextureLibrary textures(renderer, "assets");

    EXPECT_EQ(textures.get("ship.png"), ship);
    EXPECT_EQ(textures.get("ship.png"), ship);

    ASSERT_EQ(renderer.loaded.size(), 1U);
    EXPECT_EQ(renderer.loaded.front(), (std::filesystem::path("assets") / "ship.png").string());
}

TEST(TextureLibrary, PreloadReportsAMissingTextureAndDoesNotRetryIt) {
    LoadingRenderer renderer;
    TextureLibrary textures(renderer, "assets");

    EXPECT_TRUE(textures.preload("ship.png"));
    EXPECT_FALSE(textures.preload("missing.png"));
    EXPECT_EQ(textures.get("missing.png"), TextureId{});

    EXPECT_EQ(renderer.loaded.size(), 2U);
}

TEST(TextureLibrary, UnloadsItsTexturesWhenDestroyed) {
    LoadingRenderer renderer;
    {
        TextureLibrary textures(renderer, "assets");
        EXPECT_TRUE(textures.preload("ship.png"));
    }

    EXPECT_EQ(renderer.unloaded, (std::vector<TextureId>{ship}));
}
