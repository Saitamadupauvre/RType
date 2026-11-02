#include "ReceivedStrings.hpp"

#include <algorithm>
#include <ranges>
#include <string_view>
#include <utility>
#include <variant>

#include "engine/core/log/Log.hpp"

#include "FieldWire.hpp"

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

bool admits_field(const ReceivedStrings& table, const FieldState& state) {
    const core::FieldRef field = table.field(state.field_id);
    if (!field) {
        core::log::warn("network: dropped snapshot with unknown field id {}", state.field_id);
        return false;
    }
    if (field.field->kind() != core::kind_of(state.value)) {
        core::log::warn("network: dropped snapshot: field '{}' expects a {}, not a {}",
                        wire_field_name(field), core::field_kind_name(field.field->kind()),
                        core::field_kind_name(core::kind_of(state.value)));
        return false;
    }
    return true;
}

bool admits_message(const ReceivedStrings& table, const SnapshotPayload& snapshot) {
    const auto type_ids = snapshot.entities | std::views::transform(&EntityState::type_id);
    if (!admits_type_ids(table, "snapshot", type_ids)) {
        return false;
    }
    return std::ranges::all_of(snapshot.entities, [&table](const EntityState& entity) {
        return std::ranges::all_of(entity.fields, [&table](const FieldState& state) {
            return admits_field(table, state);
        });
    });
}

bool admits_message(const ReceivedStrings& table, const EventPayload&) {
    return is_after_accept(table, "event");
}

bool admits_message(const ReceivedStrings&, const auto&) { return true; }

} // namespace

bool ReceivedStrings::assign(const ConnectAcceptPayload& accept) {
    std::unordered_map<std::uint16_t, core::FieldRef> fields;
    for (const StringEntry& entry : accept.strings.entries(StringCategory::field)) {
        const core::FieldRef field = find_wire_field(_registry.get(), entry.name);
        if (!field) {
            core::log::error("network: the server replicates field '{}', unknown to this client",
                             entry.name);
            return false;
        }
        fields.emplace(entry.id, field);
    }
    _table = accept.strings;
    _fields = std::move(fields);
    return true;
}

const StringTable& ReceivedStrings::names() const noexcept {
    static const StringTable empty;
    return _table ? *_table : empty;
}

core::FieldRef ReceivedStrings::field(std::uint16_t field_id) const {
    const auto found = _fields.find(field_id);
    return found == _fields.end() ? core::FieldRef{} : found->second;
}

bool ReceivedStrings::is_known_type(std::uint16_t type_id) const {
    return _table && _table->name_of(StringCategory::type, type_id) != nullptr;
}

bool ReceivedStrings::admits(const Message& message) const {
    return std::visit([this](const auto& payload) { return admits_message(*this, payload); },
                      message);
}

} // namespace engine::network
