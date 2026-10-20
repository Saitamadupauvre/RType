#pragma once

#include <optional>

#include "engine/network/Export.hpp"

#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"

namespace engine::network {

using EncodedPayload = std::optional<Bytes>;

[[nodiscard]] ENGINE_NETWORK_EXPORT EncodedPayload
encode_payload(const ConnectRequestPayload& payload);

[[nodiscard]] ENGINE_NETWORK_EXPORT EncodedPayload
encode_payload(const ConnectAcceptPayload& payload);

[[nodiscard]] ENGINE_NETWORK_EXPORT EncodedPayload encode_payload(const InputPayload& payload);

[[nodiscard]] ENGINE_NETWORK_EXPORT EncodedPayload encode_payload(const SnapshotPayload& payload);

[[nodiscard]] ENGINE_NETWORK_EXPORT EncodedPayload encode_payload(const EventPayload& payload);

[[nodiscard]] ENGINE_NETWORK_EXPORT EncodedPayload encode_payload(const HeartbeatPayload& payload);

[[nodiscard]] ENGINE_NETWORK_EXPORT EncodedPayload encode_payload(const DisconnectPayload& payload);

} // namespace engine::network
