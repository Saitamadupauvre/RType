#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "engine/network/Export.hpp"

#include "Protocol.hpp"

namespace engine::network {

using EncodedPayload = std::optional<Bytes>;

[[nodiscard]] ENGINE_NETWORK_EXPORT EncodedPayload encode_payload(const InputPayload& payload);

[[nodiscard]] ENGINE_NETWORK_EXPORT std::optional<InputPayload>
decode_input(std::span<const std::uint8_t> payload);

[[nodiscard]] ENGINE_NETWORK_EXPORT EncodedPayload
encode_payload(const ConnectRequestPayload& payload);

[[nodiscard]] ENGINE_NETWORK_EXPORT std::optional<ConnectRequestPayload>
decode_connect_request(std::span<const std::uint8_t> payload);

[[nodiscard]] ENGINE_NETWORK_EXPORT EncodedPayload encode_payload(const HeartbeatPayload& payload);

[[nodiscard]] ENGINE_NETWORK_EXPORT std::optional<HeartbeatPayload>
decode_heartbeat(std::span<const std::uint8_t> payload);

[[nodiscard]] ENGINE_NETWORK_EXPORT EncodedPayload encode_payload(const DisconnectPayload& payload);

[[nodiscard]] ENGINE_NETWORK_EXPORT std::optional<DisconnectPayload>
decode_disconnect(std::span<const std::uint8_t> payload);

} // namespace engine::network
