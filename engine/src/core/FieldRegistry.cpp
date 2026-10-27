#include "engine/core/FieldRegistry.hpp"

#include <algorithm>
#include <format>
#include <unordered_set>

#include "engine/core/ComponentPhysics.hpp"

namespace engine::core {

namespace {

constexpr bool kinds_are_indexed_by_value() {
    for (std::size_t index = 0; index < field_kinds.size(); ++index) {
        if (static_cast<std::size_t>(field_kinds[index].kind) != index) {
            return false;
        }
    }
    return true;
}

static_assert(field_kinds.size() == std::variant_size_v<FieldValue>);
static_assert(kinds_are_indexed_by_value());
static_assert(field_kind_of<float>() == FieldKind::Float);
static_assert(field_kind_of<std::int64_t>() == FieldKind::Int);
static_assert(field_kind_of<bool>() == FieldKind::Bool);
static_assert(field_kind_of<std::string>() == FieldKind::String);
static_assert(field_kind_of<Vec2>() == FieldKind::Vec2);
static_assert(field_kind_of<Rgba>() == FieldKind::Color);
static_assert(field_kind_of<AssetName>() == FieldKind::Asset);

void require_name(std::string_view name, std::string_view what) {
    if (name.empty()) {
        throw std::invalid_argument(std::format("{} name must not be empty", what));
    }
}

void require_ref(FieldRef field) {
    if (!field) {
        throw std::invalid_argument("empty field reference");
    }
}

} // namespace

DuplicateComponentName::DuplicateComponentName(std::string_view component)
    : std::invalid_argument(std::format("component '{}' is already registered", component)) {}

DuplicateFieldName::DuplicateFieldName(std::string_view component, std::string_view field)
    : std::invalid_argument(
          std::format("component '{}' declares field '{}' twice", component, field)) {}

UnknownField::UnknownField(std::string_view component, std::string_view field)
    : std::out_of_range(std::format("unknown field '{}.{}'", component, field)) {}

FieldKindMismatch::FieldKindMismatch(std::string_view component, std::string_view field,
                                     FieldKind expected, FieldKind actual)
    : std::invalid_argument(std::format("field '{}.{}' expects a {}, not a {}", component, field,
                                        field_kind_name(expected), field_kind_name(actual))) {}

FieldValueOutOfRange::FieldValueOutOfRange(std::string_view component, std::string_view field,
                                           std::string_view reason)
    : std::out_of_range(std::format("field '{}.{}': {}", component, field, reason)) {}

const FieldDescriptor* ComponentDescriptor::find_field(std::string_view field_name) const noexcept {
    const auto found = std::ranges::find(_fields, field_name, &FieldDescriptor::name);
    return found == _fields.end() ? nullptr : &*found;
}

void FieldRegistry::add(ComponentDescriptor component) {
    require_name(component.name(), "component");
    const bool taken = std::ranges::any_of(_components, [&](const ComponentDescriptor& other) {
        return other._name == component._name || other._type == component._type;
    });
    if (taken) {
        throw DuplicateComponentName(component.name());
    }
    std::unordered_set<std::string_view> names;
    for (const FieldDescriptor& field : component.fields()) {
        require_name(field.name(), "field");
        if (!field._getter || !field._setter) {
            throw std::invalid_argument(std::format("field '{}.{}' needs a getter and a setter",
                                                    component.name(), field.name()));
        }
        if (!names.insert(field.name()).second) {
            throw DuplicateFieldName(component.name(), field.name());
        }
    }
    _components.push_back(std::move(component));
}

const ComponentDescriptor* FieldRegistry::find_component(std::string_view name) const noexcept {
    const auto found = std::ranges::find(_components, name, &ComponentDescriptor::name);
    return found == _components.end() ? nullptr : &*found;
}

FieldRef FieldRegistry::find_field(std::string_view component,
                                   std::string_view field) const noexcept {
    const ComponentDescriptor* owner = find_component(component);
    if (owner == nullptr) {
        return {};
    }
    const FieldDescriptor* found = owner->find_field(field);
    return found == nullptr ? FieldRef{} : FieldRef{.component = owner, .field = found};
}

std::span<const ComponentDescriptor> FieldRegistry::components() const noexcept {
    return _components;
}

std::vector<FieldRef> FieldRegistry::fields_of(const EntityManager& world, Entity entity) const {
    std::vector<FieldRef> fields;
    for (const ComponentDescriptor& component : _components) {
        if (!component._present(world, entity)) {
            continue;
        }
        for (const FieldDescriptor& field : component.fields()) {
            fields.push_back(FieldRef{.component = &component, .field = &field});
        }
    }
    return fields;
}

std::optional<FieldValue> FieldRegistry::get(EntityManager& world, Entity entity,
                                             FieldRef field) const {
    require_ref(field);
    if (!field.component->_present(world, entity)) {
        return std::nullopt;
    }
    return field.field->_getter(world, entity);
}

std::optional<FieldValue> FieldRegistry::get(EntityManager& world, Entity entity,
                                             std::string_view component,
                                             std::string_view field) const {
    return get(world, entity, require(component, field));
}

void FieldRegistry::set(EntityManager& world, Entity entity, FieldRef field,
                        const FieldValue& value) const {
    require_ref(field);
    const std::string& component = field.component->name();
    const FieldDescriptor& descriptor = *field.field;
    if (kind_of(value) != descriptor.kind()) {
        throw FieldKindMismatch(component, descriptor.name(), descriptor.kind(), kind_of(value));
    }
    if (!world.alive(entity)) {
        throw std::invalid_argument(
            std::format("cannot set field '{}.{}' of a dead entity", component, descriptor.name()));
    }
    try {
        descriptor._setter(world, entity, value);
    } catch (const std::out_of_range& error) {
        throw FieldValueOutOfRange(component, descriptor.name(), error.what());
    }
}

void FieldRegistry::set(EntityManager& world, Entity entity, std::string_view component,
                        std::string_view field, const FieldValue& value) const {
    set(world, entity, require(component, field), value);
}

FieldRef FieldRegistry::require(std::string_view component, std::string_view field) const {
    const FieldRef found = find_field(component, field);
    if (!found) {
        throw UnknownField(component, field);
    }
    return found;
}

void register_core_fields(FieldRegistry& registry) {
    registry.add<Position>("Position",
                           {member_field<&Position::pos>("pos", field_flags::replicated)});
    registry.add<Velocity>("Velocity",
                           {member_field<&Velocity::vel>("vel", field_flags::replicated)});
    registry.add<Sprite>("Sprite",
                         {asset_field<&Sprite::path>("path", field_flags::editable),
                          member_field<&Sprite::color>("color", field_flags::replicated)});
}

} // namespace engine::core
