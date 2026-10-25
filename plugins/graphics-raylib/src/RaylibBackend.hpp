#pragma once

#include "RaylibInput.hpp"
#include "RaylibRenderer.hpp"
#include "engine/graphics/IGraphicsBackend.hpp"

class RaylibBackend final : public engine::graphics::IGraphicsBackend {
public:
    engine::graphics::IRenderer& renderer() override { return _renderer; }
    engine::graphics::IInput& input() override { return _input; }

private:
    RaylibRenderer _renderer;
    RaylibInput _input;
};