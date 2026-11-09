#pragma once

#include <optional>

#include "engine/core/event/EventBus.hpp"
#include "engine/network/Events.hpp"
#include "engine/network/Export.hpp"
#include "engine/network/StringTable.hpp"

#include "protocol/Messages.hpp"

namespace engine::network {

[[nodiscard]] ENGINE_NETWORK_EXPORT EventPayload to_payload(const SoundPlayed& event) noexcept;
[[nodiscard]] ENGINE_NETWORK_EXPORT EventPayload to_payload(const MusicChanged& event) noexcept;
[[nodiscard]] ENGINE_NETWORK_EXPORT std::optional<EventPayload>
to_payload(const NamedEvent& event, const StringTable& strings);

ENGINE_NETWORK_EXPORT void publish_received(core::event::EventBus& events,
                                            const EventPayload& event, const StringTable& strings);

} // namespace engine::network
