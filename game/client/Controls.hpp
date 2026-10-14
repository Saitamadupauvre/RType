#pragma once

#include "engine/graphics/IInput.hpp"
#include "engine/script/InputState.hpp"

namespace rtype::client {

engine::script::InputState read_controls(const engine::graphics::IInput& input);

} // namespace rtype::client
