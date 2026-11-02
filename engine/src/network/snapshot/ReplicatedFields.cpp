#include "ReplicatedFields.hpp"

#include <format>
#include <optional>
#include <utility>

#include "engine/core/log/Log.hpp"

#include "protocol/FieldWire.hpp"
#include "protocol/Protocol.hpp"

namespace engine::network {

namespace {

std::optional<std::uint16_t> field_id(StringTable& strings, const std::string& name) {
    if (const auto known = strings.id_of(StringCategory::field, name)) {
        return known;
    }
    return strings.add(StringCategory::field, name);
}

} // namespace

ReplicatedFields::ReplicatedFields(const core::FieldRegistry& registry, StringTable& strings)
    : _registry(registry) {
    for (const core::ComponentDescriptor& component : registry.components()) {
        for (const core::FieldDescriptor& field : component.fields()) {
            if (field.flags().replicated && _error.empty()) {
                declare(core::FieldRef{.component = &component, .field = &field}, strings);
            }
        }
    }
    if (_error.empty()) {
        check_entity_size();
    }
}

void ReplicatedFields::declare(core::FieldRef field, StringTable& strings) {
    const auto name = wire_field_name(field);
    if (!is_replicable(field.field->kind())) {
        fail(std::format("field '{}' is a {}, which snapshots cannot carry", name,
                         core::field_kind_name(field.field->kind())));
        return;
    }
    if (_fields.size() == max_entity_fields) {
        fail(std::format("more than {} fields are replicated", max_entity_fields));
        return;
    }
    const auto id = field_id(strings, name);
    if (!id) {
        fail(std::format("field '{}' cannot be added to the string table", name));
        return;
    }
    _fields.push_back(ReplicatedField{.id = *id, .field = field});
}

void ReplicatedFields::check_entity_size() {
    std::size_t size = entity_header_size;
    for (const ReplicatedField& replicated : _fields) {
        size += field_header_size + max_wire_size(replicated.field.field->kind());
    }
    if (size > max_snapshot_entity_bytes) {
        fail(std::format("an entity with every replicated field takes {} bytes, more than the {} "
                         "a snapshot holds",
                         size, max_snapshot_entity_bytes));
    }
}

void ReplicatedFields::fail(std::string error) {
    _error = std::move(error);
    _fields.clear();
}

std::vector<FieldState> ReplicatedFields::read(core::EntityManager& world,
                                               core::Entity entity) const {
    std::vector<FieldState> states;
    for (const ReplicatedField& replicated : _fields) {
        auto value = _registry.get().get(world, entity, replicated.field);
        if (!value) {
            continue;
        }
        if (!fits_wire(*value)) {
            core::log::warn("network: field '{}' of entity {} cannot be sent: value out of range",
                            wire_field_name(replicated.field), entity.index());
            continue;
        }
        states.push_back(FieldState{.field_id = replicated.id, .value = std::move(*value)});
    }
    return states;
}

} // namespace engine::network
