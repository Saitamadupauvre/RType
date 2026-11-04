#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <type_traits>

#include "engine/core/math/Vec2.hpp"
#include "engine/graphics/IGraphicsBackend.hpp"
#include "engine/graphics/PluginApi.hpp"
#include "fixtures/NullRenderer.hpp"

using namespace engine::graphics;
using namespace engine::core;

namespace {

class FakeRenderer final : public fixture::NullRenderer {
public:
    bool open(const WindowConfig& config) override {
        _open = config.width > 0 && config.height > 0;
        return _open;
    }
    void close() override { _open = false; }
    [[nodiscard]] bool should_close() const override { return !_open; }
    void set_fullscreen(bool enabled) override { fullscreen = enabled; }
    void begin_frame(Color clear) override { last_clear = clear; }
    void end_frame() override { ++frames; }

    bool fullscreen{false};
    engine::graphics::Color last_clear{};
    int frames{0};

private:
    bool _open{false};
};

class FakeInput final : public IInput {
public:
    [[nodiscard]] bool is_key_down(Key key) const override { return key == down; }
    [[nodiscard]] bool is_key_pressed(Key) const override { return false; }
    [[nodiscard]] bool is_key_released(Key) const override { return false; }
    [[nodiscard]] Key last_key_pressed() override { return Key::Unknown; }
    [[nodiscard]] Vec2 mouse_position() const override { return {}; }
    [[nodiscard]] bool is_mouse_down(engine::graphics::MouseButton) const override { return false; }
    [[nodiscard]] bool is_mouse_pressed(engine::graphics::MouseButton) const override {
        return false;
    }
    [[nodiscard]] bool is_mouse_released(engine::graphics::MouseButton) const override {
        return false;
    }

    Key down{Key::Unknown};
};

class FakeBackend final : public IGraphicsBackend {
public:
    explicit FakeBackend(bool& destroyed) : _destroyed(destroyed) {}
    ~FakeBackend() override { _destroyed = true; }

    IRenderer& renderer() override { return _renderer; }
    IInput& input() override { return _input; }

private:
    bool& _destroyed;
    FakeRenderer _renderer;
    FakeInput _input;
};

bool fake_destroyed = false;

std::uint32_t fake_api_version() { return plugin_api_version; }
IGraphicsBackend* fake_create() { return new FakeBackend(fake_destroyed); }
void fake_destroy(IGraphicsBackend* backend) { delete backend; }

} // namespace

TEST(GraphicsBackend, DestroyingThroughInterfaceRunsDerivedDestructor) {
    bool destroyed = false;
    std::unique_ptr<IGraphicsBackend> backend = std::make_unique<FakeBackend>(destroyed);

    backend.reset();

    EXPECT_TRUE(destroyed);
}

TEST(GraphicsBackend, ExposesRendererAndInputThroughInterfaces) {
    bool destroyed = false;
    FakeBackend fake(destroyed);
    IGraphicsBackend& backend = fake;

    ASSERT_TRUE(backend.renderer().open(WindowConfig{}));
    backend.renderer().begin_frame(engine::graphics::Color{.r = 1, .g = 2, .b = 3});
    backend.renderer().end_frame();

    EXPECT_FALSE(backend.renderer().should_close());
    EXPECT_EQ(backend.renderer().virtual_size(), (Vec2{1920, 1080}));
    EXPECT_FALSE(backend.input().is_key_down(Key::Space));
}

TEST(GraphicsBackend, ConvertsThroughViewInVirtualSpace) {
    bool destroyed = false;
    FakeBackend fake(destroyed);
    const IRenderer& renderer = fake.renderer();
    const CameraView view{.center = {100.F, 200.F}, .zoom = 2.F};

    EXPECT_EQ(renderer.world_to_screen(view, {100.F, 200.F}), (Vec2{960.F, 540.F}));
    EXPECT_EQ(renderer.world_to_screen(view, {110.F, 200.F}), (Vec2{980.F, 540.F}));
    EXPECT_EQ(renderer.screen_to_world(view, {980.F, 540.F}), (Vec2{110.F, 200.F}));
}

TEST(GraphicsBackend, DefaultViewConvertsToSamePoint) {
    bool destroyed = false;
    FakeBackend fake(destroyed);
    const IRenderer& renderer = fake.renderer();
    const CameraView view = default_view(renderer.virtual_size());

    EXPECT_EQ(renderer.world_to_screen(view, {37.F, 1000.F}), (Vec2{37.F, 1000.F}));
    EXPECT_EQ(renderer.screen_to_world(view, {37.F, 1000.F}), (Vec2{37.F, 1000.F}));
}

TEST(GraphicsBackend, InterfacesAreNotCopyableOrMovable) {
    static_assert(!std::is_copy_constructible_v<IRenderer>);
    static_assert(!std::is_move_constructible_v<IRenderer>);
    static_assert(!std::is_copy_constructible_v<IInput>);
    static_assert(!std::is_move_constructible_v<IInput>);
    static_assert(!std::is_copy_constructible_v<IGraphicsBackend>);
    static_assert(!std::is_move_constructible_v<IGraphicsBackend>);
    SUCCEED();
}

TEST(PluginApi, EntryPointsMatchFunctionTypes) {
    const ApiVersionFunction version = &fake_api_version;
    const CreateFunction create = &fake_create;
    const DestroyFunction destroy = &fake_destroy;

    EXPECT_EQ(version(), plugin_api_version);
    std::unique_ptr<IGraphicsBackend, DestroyFunction> backend(create(), destroy);
    ASSERT_NE(backend, nullptr);
    fake_destroyed = false;
    backend.reset();
    EXPECT_TRUE(fake_destroyed);
}

TEST(PluginApi, SymbolNamesAreStable) {
    EXPECT_EQ(api_version_symbol, "engine_graphics_api_version");
    EXPECT_EQ(create_symbol, "engine_graphics_create");
    EXPECT_EQ(destroy_symbol, "engine_graphics_destroy");
}
