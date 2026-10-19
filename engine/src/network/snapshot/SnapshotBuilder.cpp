#include "SnapshotBuilder.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/network/Replicated.hpp"

#include "protocol/Protocol.hpp"

namespace engine::network {

namespace {

core::Vec2 velocity_of(core::EntityManager& registry, core::Entity entity) {
    if (!registry.is_registered<core::Velocity>() || !registry.has<core::Velocity>(entity)) {
        return {};
    }
    return registry.get<core::Velocity>(entity).vel;
}

std::vector<EntityState> replicated_states(core::EntityManager& registry) {
    std::vector<EntityState> states;
    if (!registry.is_registered<Replicated>() || !registry.is_registered<core::Position>()) {
        return states;
    }

    registry.query<Replicated, core::Position>().each(
        [&](core::Entity entity, const Replicated& replicated, const core::Position& position) {
            const auto velocity = velocity_of(registry, entity);
            states.push_back(EntityState{.entity_id = entity.index(),
                                         .type_id = replicated.type_id,
                                         .x = to_wire_coordinate(position.pos.x),
                                         .y = to_wire_coordinate(position.pos.y),
                                         .vx = to_wire_coordinate(velocity.x),
                                         .vy = to_wire_coordinate(velocity.y)});
        });
    return states;
}

} // namespace

std::int16_t to_wire_coordinate(float value) noexcept {
    if (std::isnan(value)) {
        return 0;
    }
    constexpr auto lowest = static_cast<float>(std::numeric_limits<std::int16_t>::min());
    constexpr auto highest = static_cast<float>(std::numeric_limits<std::int16_t>::max());
    return static_cast<std::int16_t>(std::clamp(std::round(value), lowest, highest));
}

std::vector<SnapshotPayload> build_snapshots(core::EntityManager& registry,
                                             std::uint32_t server_tick) {
    const auto states = replicated_states(registry);
    std::vector<SnapshotPayload> snapshots;

    auto next = states.begin();
    do {
        const auto count =
            std::min<std::ptrdiff_t>(std::distance(next, states.end()), max_snapshot_entities);
        snapshots.push_back(
            SnapshotPayload{.server_tick = server_tick, .entities = {next, next + count}});
        next += count;
    } while (next != states.end());

    return snapshots;
}

} // namespace engine::network
