#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "engine/network/Export.hpp"

#include "protocol/Messages.hpp"

namespace engine::network {

[[nodiscard]] ENGINE_NETWORK_EXPORT std::optional<ConnectRequestPayload>
decode_connect_request(std::span<const std::uint8_t> payload);

[[nodiscard]] ENGINE_NETWORK_EXPORT std::optional<ConnectAcceptPayload>
decode_connect_accept(std::span<const std::uint8_t> payload);

[[nodiscard]] ENGINE_NETWORK_EXPORT std::optional<InputPayload>
decode_input(std::span<const std::uint8_t> payload);

[[nodiscard]] ENGINE_NETWORK_EXPORT std::optional<SnapshotPayload>
decode_snapshot(std::span<const std::uint8_t> payload);

[[nodiscard]] ENGINE_NETWORK_EXPORT std::optional<EventPayload>
decode_event(std::span<const std::uint8_t> payload);

[[nodiscard]] ENGINE_NETWORK_EXPORT std::optional<HeartbeatPayload>
decode_heartbeat(std::span<const std::uint8_t> payload);

[[nodiscard]] ENGINE_NETWORK_EXPORT std::optional<DisconnectPayload>
decode_disconnect(std::span<const std::uint8_t> payload);

} // namespace engine::network
