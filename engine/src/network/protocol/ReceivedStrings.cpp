#include "ReceivedStrings.hpp"

#include <algorithm>
#include <ranges>
#include <string_view>
#include <variant>

#include "engine/core/log/Log.hpp"

namespace engine::network {

namespace {

bool is_after_accept(const ReceivedStrings& table, std::string_view message_name) {
    if (!table.is_assigned()) {
        core::log::warn("network: dropped {} received before connect_accept", message_name);
        return false;
    }
    return true;
}

bool admits_type_ids(const ReceivedStrings& table, std::string_view message_name,
                     const auto& type_ids) {
    if (!is_after_accept(table, message_name)) {
        return false;
    }
    const auto unknown = std::ranges::find_if(
        type_ids, [&table](std::uint16_t type_id) { return !table.is_known_type(type_id); });
    if (unknown != std::ranges::end(type_ids)) {
        core::log::warn("network: dropped {} with unknown type id {}", message_name, *unknown);
        return false;
    }
    return true;
}

bool admits_message(const ReceivedStrings& table, const SnapshotPayload& snapshot) {
    const auto type_ids = snapshot.entities | std::views::transform(&EntityState::type_id);
    return admits_type_ids(table, "snapshot", type_ids);
}

bool admits_message(const ReceivedStrings& table, const EventPayload& event) {
    if (const auto* spawn = std::get_if<SpawnEvent>(&event)) {
        return admits_type_ids(table, "spawn event", std::views::single(spawn->type_id));
    }
    return is_after_accept(table, "event");
}

bool admits_message(const ReceivedStrings&, const auto&) { return true; }

} // namespace

void ReceivedStrings::assign(const ConnectAcceptPayload& accept) { _table = accept.strings; }

bool ReceivedStrings::is_known_type(std::uint16_t type_id) const {
    return _table && _table->name_of(StringCategory::type, type_id) != nullptr;
}

bool ReceivedStrings::admits(const Message& message) const {
    return std::visit([this](const auto& payload) { return admits_message(*this, payload); },
                      message);
}

} // namespace engine::network
