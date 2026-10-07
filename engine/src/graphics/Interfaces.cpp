#include "engine/graphics/IGraphicsBackend.hpp"
#include "engine/graphics/IInput.hpp"
#include "engine/graphics/IRenderer.hpp"

namespace engine::graphics {

IRenderer::~IRenderer() = default;

IInput::~IInput() = default;

IGraphicsBackend::~IGraphicsBackend() = default;

} // namespace engine::graphics
