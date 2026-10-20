#pragma once

#include "engine/core/event/EventBus.hpp"
#include "engine/network/Events.hpp"
#include "engine/network/Export.hpp"

#include "protocol/Messages.hpp"

namespace engine::network {

[[nodiscard]] ENGINE_NETWORK_EXPORT EventPayload to_payload(const EntitySpawned& event) noexcept;
[[nodiscard]] ENGINE_NETWORK_EXPORT EventPayload to_payload(const EntityDestroyed& event) noexcept;
[[nodiscard]] ENGINE_NETWORK_EXPORT EventPayload to_payload(const ShotFired& event) noexcept;
[[nodiscard]] ENGINE_NETWORK_EXPORT EventPayload to_payload(const SoundPlayed& event) noexcept;

ENGINE_NETWORK_EXPORT void publish_received(core::event::EventBus& events,
                                            const EventPayload& event);

} // namespace engine::network
