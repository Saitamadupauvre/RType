#pragma once

#include <optional>

#include "engine/core/FieldRegistry.hpp"
#include "engine/network/Export.hpp"
#include "engine/network/StringTable.hpp"

#include "Messages.hpp"

namespace engine::network {

[[nodiscard]] ENGINE_NETWORK_EXPORT core::FieldKind kind_of(const WireValue& value) noexcept;

[[nodiscard]] ENGINE_NETWORK_EXPORT std::optional<WireValue> to_wire(const core::FieldValue& value,
                                                                     const StringTable& strings);

[[nodiscard]] ENGINE_NETWORK_EXPORT std::optional<core::FieldValue>
from_wire(const WireValue& value, const StringTable& strings);

} // namespace engine::network
