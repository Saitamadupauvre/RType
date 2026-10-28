#pragma once

#include "Messages.hpp"

namespace engine::network {

[[nodiscard]] bool is_valid(const InputPayload& payload) noexcept;
[[nodiscard]] bool is_valid(const SnapshotPayload& payload);
[[nodiscard]] bool is_valid(const ConnectAcceptPayload& payload);
[[nodiscard]] bool is_valid(const DisconnectPayload& payload) noexcept;
[[nodiscard]] bool is_valid(const NamedEventState& event);
[[nodiscard]] bool is_valid(const EventPayload& payload);

} // namespace engine::network
