#include "SnapshotBuilder.hpp"

#include <cstddef>
#include <utility>

#include "engine/network/Replicated.hpp"

#include "protocol/FieldWire.hpp"
#include "protocol/Protocol.hpp"

namespace engine::network {

namespace {

std::vector<EntityState> replicated_states(core::EntityManager& world,
                                           const ReplicatedFields& fields) {
    std::vector<EntityState> states;
    if (!world.is_registered<Replicated>()) {
        return states;
    }
    world.query<Replicated>().each([&](core::Entity entity, const Replicated& replicated) {
        states.push_back(EntityState{.entity_id = entity.index(),
                                     .type_id = replicated.type_id,
                                     .fields = fields.read(world, entity)});
    });
    return states;
}

} // namespace

std::vector<SnapshotPayload> build_snapshots(core::EntityManager& world,
                                             const ReplicatedFields& fields,
                                             std::uint32_t server_tick) {
    std::vector<SnapshotPayload> snapshots{
        SnapshotPayload{.server_tick = server_tick, .entities = {}}};
    std::size_t used = 0;
    for (auto& state : replicated_states(world, fields)) {
        const auto size = wire_size(state);
        if (used + size > max_snapshot_entity_bytes) {
            snapshots.push_back(SnapshotPayload{.server_tick = server_tick, .entities = {}});
            used = 0;
        }
        used += size;
        snapshots.back().entities.push_back(std::move(state));
    }
    return snapshots;
}

} // namespace engine::network
