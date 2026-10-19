#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "engine/network/Export.hpp"

#include "protocol/Messages.hpp"

namespace engine::network {

class ENGINE_NETWORK_EXPORT SnapshotReceiver {
public:
    bool receive(const SnapshotPayload& snapshot);

    [[nodiscard]] std::optional<std::uint32_t> server_tick() const noexcept { return tick_; }
    [[nodiscard]] const std::vector<EntityState>& entities() const noexcept { return entities_; }

private:
    void merge(const std::vector<EntityState>& entities);

    std::optional<std::uint32_t> tick_;
    std::vector<EntityState> entities_;
};

} // namespace engine::network
