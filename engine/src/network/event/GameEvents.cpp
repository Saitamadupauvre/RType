#include "GameEvents.hpp"

#include <string>
#include <variant>

#include "engine/core/log/Log.hpp"

#include "protocol/Validation.hpp"

namespace engine::network {

namespace {

void publish(core::event::EventBus& events, const SoundEvent& event, const StringTable&) {
    events.publish(
        SoundPlayed{.source_entity_id = event.source_entity_id, .sound_id = event.sound_id});
}

void publish(core::event::EventBus& events, const PlayerLeftEvent& event, const StringTable&) {
    events.publish(PlayerLeft{.player_id = event.player_entity_id});
}

void publish(core::event::EventBus& events, const MusicEvent& event, const StringTable&) {
    events.publish(MusicChanged{.music_id = event.music_id});
}

void publish(core::event::EventBus& events, const NamedEventState& event,
             const StringTable& strings) {
    const std::string* name = strings.name_of(StringCategory::event, event.name_id);
    if (name == nullptr) {
        core::log::warn("network: dropped event with unknown name id {}", event.name_id);
        return;
    }
    NamedEvent received{.name = *name, .arguments = {}};
    received.arguments.reserve(event.arguments.size());
    for (const EventArgumentState& argument : event.arguments) {
        const std::string* key = strings.name_of(StringCategory::event_key, argument.key_id);
        if (key == nullptr) {
            core::log::warn("network: dropped event '{}' with unknown key id {}", *name,
                            argument.key_id);
            return;
        }
        received.arguments.push_back({.key = *key, .value = argument.value});
    }
    events.publish(received);
}

} // namespace

EventPayload to_payload(const SoundPlayed& event) noexcept {
    return SoundEvent{.source_entity_id = event.source_entity_id, .sound_id = event.sound_id};
}

EventPayload to_payload(const MusicChanged& event) noexcept {
    return MusicEvent{.music_id = event.music_id};
}

std::optional<EventPayload> to_payload(const NamedEvent& event, const StringTable& strings) {
    const auto name_id = strings.id_of(StringCategory::event, event.name);
    if (!name_id) {
        core::log::warn("network: dropped event '{}': not in the string table", event.name);
        return std::nullopt;
    }
    NamedEventState state{.name_id = *name_id, .arguments = {}};
    state.arguments.reserve(event.arguments.size());
    for (const NamedEvent::Argument& argument : event.arguments) {
        const auto key_id = strings.id_of(StringCategory::event_key, argument.key);
        if (!key_id) {
            core::log::warn("network: dropped event '{}': key '{}' is not in the string table",
                            event.name, argument.key);
            return std::nullopt;
        }
        state.arguments.push_back({.key_id = *key_id, .value = argument.value});
    }
    if (!is_valid(state)) {
        core::log::warn("network: dropped event '{}': more than {} arguments, a repeated key, a "
                        "list, a non-finite float or a text longer than {} bytes",
                        event.name, max_event_arguments, max_event_text_length);
        return std::nullopt;
    }
    return state;
}

void publish_received(core::event::EventBus& events, const EventPayload& event,
                      const StringTable& strings) {
    std::visit([&](const auto& payload) { publish(events, payload, strings); }, event);
}

} // namespace engine::network
