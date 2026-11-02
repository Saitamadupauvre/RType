#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>

#include "engine/core/FieldRegistry.hpp"
#include "engine/network/Export.hpp"
#include "engine/network/StringTable.hpp"

#include "protocol/Messages.hpp"

namespace engine::network {

class ENGINE_NETWORK_EXPORT ReceivedStrings {
public:
    explicit ReceivedStrings(const core::FieldRegistry& fields) : _registry(fields) {}

    bool assign(const ConnectAcceptPayload& accept);

    [[nodiscard]] bool is_assigned() const noexcept { return _table.has_value(); }
    [[nodiscard]] bool is_known_type(std::uint16_t type_id) const;
    [[nodiscard]] core::FieldRef field(std::uint16_t field_id) const;
    [[nodiscard]] bool admits(const Message& message) const;
    [[nodiscard]] StringTable table() const { return _table.value_or(StringTable{}); }
    [[nodiscard]] const StringTable& names() const noexcept;

private:
    std::reference_wrapper<const core::FieldRegistry> _registry;
    std::optional<StringTable> _table;
    std::unordered_map<std::uint16_t, core::FieldRef> _fields;
};

} // namespace engine::network
