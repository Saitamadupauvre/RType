#include "TypeTable.hpp"

#include <algorithm>
#include <ranges>
#include <string_view>
#include <variant>

#include "engine/core/log/Log.hpp"

namespace engine::network {

namespace {

bool is_after_accept(const TypeTable& table, std::string_view message_name) {
    if (!table.is_assigned()) {
        core::log::warn("network: dropped {} received before connect_accept", message_name);
        return false;
    }
    return true;
}

bool admits_type_ids(const TypeTable& table, std::string_view message_name, const auto& type_ids) {
    if (!is_after_accept(table, message_name)) {
        return false;
    }
    const auto unknown = std::ranges::find_if(
        type_ids, [&table](std::uint16_t type_id) { return !table.is_known(type_id); });
    if (unknown != std::ranges::end(type_ids)) {
        core::log::warn("network: dropped {} with unknown type id {}", message_name, *unknown);
        return false;
    }
    return true;
}

bool admits_message(const TypeTable& table, const SnapshotPayload& snapshot) {
    const auto type_ids = snapshot.entities | std::views::transform(&EntityState::type_id);
    return admits_type_ids(table, "snapshot", type_ids);
}

bool admits_message(const TypeTable& table, const EventPayload& event) {
    if (const auto* spawn = std::get_if<SpawnEvent>(&event)) {
        return admits_type_ids(table, "spawn event", std::views::single(spawn->type_id));
    }
    return is_after_accept(table, "event");
}

bool admits_message(const TypeTable&, const auto&) { return true; }

} // namespace

void TypeTable::assign(const ConnectAcceptPayload& accept) {
    auto& type_ids = type_ids_.emplace();
    for (const auto& entry : accept.types) {
        type_ids.insert(entry.type_id);
    }
}

bool TypeTable::is_known(std::uint16_t type_id) const noexcept {
    return type_ids_ && type_ids_->contains(type_id);
}

bool TypeTable::admits(const Message& message) const {
    return std::visit([this](const auto& payload) { return admits_message(*this, payload); },
                      message);
}

} // namespace engine::network
