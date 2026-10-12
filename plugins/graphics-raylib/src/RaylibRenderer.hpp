#pragma once

#include <raylib.h>

#include "engine/graphics/IRenderer.hpp"
#include "engine/graphics/Types.hpp"

class RaylibRenderer final : public engine::graphics::IRenderer {
public:
    RaylibRenderer() = default;
    ~RaylibRenderer() override;

    bool open(const engine::graphics::WindowConfig& config) override;
    void close() override;
    [[nodiscard]] bool should_close() const override;
    void set_fullscreen(bool enabled) override;
    [[nodiscard]] engine::graphics::Vec2 virtual_size() const override;
    void begin_frame(engine::graphics::Color clear) override;
    void end_frame() override;

private:
    bool _open{false};
    RenderTexture2D _target{};
    engine::graphics::Color _clear{};
};
