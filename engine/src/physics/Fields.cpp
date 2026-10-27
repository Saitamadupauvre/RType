#include "engine/physics/Fields.hpp"

#include <cmath>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string_view>

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

void require_finite(core::Vec2 value, std::string_view what) {
    if (!std::isfinite(value.x) || !std::isfinite(value.y)) {
        throw std::out_of_range(std::format("{} must be finite", what));
    }
}

core::FieldDescriptor aabb_offset_field() {
    return core::component_field<AABBCollider>(
        "offset", core::FieldKind::Vec2, editable,
        [](const AABBCollider& collider) { return core::FieldValue{collider.offset}; },
        [](AABBCollider& collider, const core::FieldValue& value) {
            const auto offset = std::get<core::Vec2>(value);
            require_finite(offset, "offset");
            collider.offset = offset;
        });
}

core::FieldDescriptor aabb_size_field() {
    return core::component_field<AABBCollider>(
        "size", core::FieldKind::Vec2, editable,
        [](const AABBCollider& collider) { return core::FieldValue{collider.size}; },
        [](AABBCollider& collider, const core::FieldValue& value) {
            const auto size = std::get<core::Vec2>(value);
            require_finite(size, "size");
            if (size.x <= 0.0F || size.y <= 0.0F) {
                throw std::out_of_range("size must be positive");
            }
            collider.size = size;
        });
}

core::FieldDescriptor circle_offset_field() {
    return core::component_field<CircleCollider>(
        "offset", core::FieldKind::Vec2, editable,
        [](const CircleCollider& collider) { return core::FieldValue{collider.offset}; },
        [](CircleCollider& collider, const core::FieldValue& value) {
            const auto offset = std::get<core::Vec2>(value);
            require_finite(offset, "offset");
            collider.offset = offset;
        });
}

core::FieldDescriptor circle_radius_field() {
    return core::component_field<CircleCollider>(
        "radius", core::FieldKind::Float, editable,
        [](const CircleCollider& collider) { return core::FieldValue{collider.radius}; },
        [](CircleCollider& collider, const core::FieldValue& value) {
            const auto radius = std::get<float>(value);
            if (!std::isfinite(radius) || radius <= 0.0F) {
                throw std::out_of_range("radius must be positive");
            }
            collider.radius = radius;
        });
}

core::FieldDescriptor collides_with_field() {
    return core::list_field<CollidesWith>(
        "labels", core::FieldKind::String, editable,
        [](const CollidesWith& filter) { return core::labels_to_list(filter.tags); },
        [](CollidesWith& filter, const core::FieldList& list) {
            filter.tags = core::labels_from_list(list);
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
    registry.add<AABBCollider>("AABBCollider", {aabb_offset_field(), aabb_size_field()},
                               core::required<PhysicsBody>);
    registry.add<CircleCollider>("CircleCollider", {circle_offset_field(), circle_radius_field()},
                                 core::required<PhysicsBody>);
    registry.add<CollidesWith>("CollidesWith", {collides_with_field()});
}

} // namespace engine::physics
