#pragma once

#include "engine/core/InputState.hpp"
#include "engine/network/Export.hpp"

#include "protocol/Messages.hpp"

namespace engine::network {

[[nodiscard]] ENGINE_NETWORK_EXPORT InputPayload to_payload(const core::InputState& input);
[[nodiscard]] ENGINE_NETWORK_EXPORT core::InputState to_input_state(const InputPayload& payload);

} // namespace engine::network
