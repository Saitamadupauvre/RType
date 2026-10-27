#include "engine/physics/Fields.hpp"

#include <cstdint>
#include <stdexcept>

#include "engine/physics/Components.hpp"

namespace engine::physics {

namespace {

using core::field_flags::editable;

core::FieldDescriptor body_type_field() {
    return core::component_field<PhysicsBody>(
        "type", core::FieldKind::Int, editable,
        [](const PhysicsBody& body) {
            return core::FieldValue{static_cast<std::int64_t>(body.type)};
        },
        [](PhysicsBody& body, const core::FieldValue& value) {
            const auto type = std::get<std::int64_t>(value);
            if (type != static_cast<std::int64_t>(PhysicsBody::Type::Static) &&
                type != static_cast<std::int64_t>(PhysicsBody::Type::Dynamic)) {
                throw std::out_of_range("body type must be 0 (static) or 1 (dynamic)");
            }
            body.type = static_cast<PhysicsBody::Type>(type);
        });
}

} // namespace

void register_physics_fields(core::FieldRegistry& registry) {
    registry.add<PhysicsBody>(
        "PhysicsBody",
        {body_type_field(), core::member_field<&PhysicsBody::velocity>("velocity", editable),
         core::member_field<&PhysicsBody::acceleration>("acceleration", editable),
         core::member_field<&PhysicsBody::mass>("mass", editable),
         core::member_field<&PhysicsBody::resolve_collisions>("resolve_collisions", editable),
         core::member_field<&PhysicsBody::affected_by_gravity>("affected_by_gravity", editable),
         core::member_field<&PhysicsBody::collision_layer>("collision_layer", editable),
         core::member_field<&PhysicsBody::collision_mask>("collision_mask", editable)});
    registry.add<AABBCollider>("AABBCollider",
                               {core::member_field<&AABBCollider::offset>("offset", editable),
                                core::member_field<&AABBCollider::size>("size", editable)});
    registry.add<CircleCollider>("CircleCollider",
                                 {core::member_field<&CircleCollider::offset>("offset", editable),
                                  core::member_field<&CircleCollider::radius>("radius", editable)});
}

} // namespace engine::physics
