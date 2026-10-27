#pragma once

#include <cstdint>
#include <optional>

#include "engine/network/Export.hpp"
#include "engine/network/StringTable.hpp"

#include "protocol/Messages.hpp"

namespace engine::network {

class ENGINE_NETWORK_EXPORT ReceivedStrings {
public:
    void assign(const ConnectAcceptPayload& accept);

    [[nodiscard]] bool is_assigned() const noexcept { return _table.has_value(); }
    [[nodiscard]] bool is_known_type(std::uint16_t type_id) const;
    [[nodiscard]] bool admits(const Message& message) const;
    [[nodiscard]] StringTable table() const { return _table.value_or(StringTable{}); }

private:
    std::optional<StringTable> _table;
};

} // namespace engine::network
