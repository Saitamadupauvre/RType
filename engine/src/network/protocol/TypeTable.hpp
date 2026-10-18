#pragma once

#include <cstdint>
#include <optional>
#include <unordered_set>

#include "engine/network/Export.hpp"

#include "protocol/Messages.hpp"

namespace engine::network {

class ENGINE_NETWORK_EXPORT TypeTable {
public:
    void assign(const ConnectAcceptPayload& accept);

    [[nodiscard]] bool is_assigned() const noexcept { return type_ids_.has_value(); }
    [[nodiscard]] bool is_known(std::uint16_t type_id) const noexcept;
    [[nodiscard]] bool admits(const Message& message) const;

private:
    std::optional<std::unordered_set<std::uint16_t>> type_ids_;
};

} // namespace engine::network
