#include "SnapshotReceiver.hpp"

#include <algorithm>

#include "protocol/Sequence.hpp"

namespace engine::network {

bool SnapshotReceiver::receive(const SnapshotPayload& snapshot) {
    if (tick_ && is_newer_sequence(*tick_, snapshot.server_tick)) {
        return false;
    }
    if (tick_ != snapshot.server_tick) {
        tick_ = snapshot.server_tick;
        entities_.clear();
    }
    merge(snapshot.entities);
    return true;
}

void SnapshotReceiver::merge(const std::vector<EntityState>& entities) {
    for (const auto& entity : entities) {
        const auto known = std::ranges::find(entities_, entity.entity_id, &EntityState::entity_id);
        if (known == entities_.end()) {
            entities_.push_back(entity);
        } else {
            *known = entity;
        }
    }
}

} // namespace engine::network
