#include "engine/graphics/IGraphicsBackend.hpp"
#include "engine/graphics/IInput.hpp"
#include "engine/graphics/IRenderer.hpp"

namespace engine::graphics {

IRenderer::~IRenderer() = default;

core::Vec2 IRenderer::world_to_screen(const core::CameraView& view, core::Vec2 point) const {
    return core::world_to_screen(view, virtual_size(), point);
}

core::Vec2 IRenderer::screen_to_world(const core::CameraView& view, core::Vec2 point) const {
    return core::screen_to_world(view, virtual_size(), point);
}

IInput::~IInput() = default;

IGraphicsBackend::~IGraphicsBackend() = default;

} // namespace engine::graphics
