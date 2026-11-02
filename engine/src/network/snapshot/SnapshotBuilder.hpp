#pragma once

#include <cstdint>
#include <vector>

#include "engine/core/EntityManager.hpp"
#include "engine/network/Export.hpp"

#include "ReplicatedFields.hpp"
#include "protocol/Messages.hpp"

namespace engine::network {

[[nodiscard]] ENGINE_NETWORK_EXPORT std::vector<SnapshotPayload>
build_snapshots(core::EntityManager& world, const ReplicatedFields& fields,
                std::uint32_t server_tick);

} // namespace engine::network
