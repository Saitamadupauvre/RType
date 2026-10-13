#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "engine/network/Export.hpp"

#include "Protocol.hpp"

namespace engine::network {

[[nodiscard]] ENGINE_NETWORK_EXPORT std::optional<Bytes>
encode_payload(const InputPayload& payload);

[[nodiscard]] ENGINE_NETWORK_EXPORT std::optional<InputPayload>
decode_input(std::span<const std::uint8_t> payload);

} // namespace engine::network
