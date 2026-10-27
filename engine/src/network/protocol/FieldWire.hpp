#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "engine/core/FieldRegistry.hpp"
#include "engine/network/Export.hpp"

#include "Messages.hpp"

namespace engine::network {

[[nodiscard]] ENGINE_NETWORK_EXPORT std::int16_t to_wire_coordinate(float value) noexcept;
[[nodiscard]] ENGINE_NETWORK_EXPORT std::int32_t to_wire_int(std::int64_t value) noexcept;

[[nodiscard]] ENGINE_NETWORK_EXPORT std::string wire_field_name(core::FieldRef field);
[[nodiscard]] ENGINE_NETWORK_EXPORT core::FieldRef
find_wire_field(const core::FieldRegistry& registry, std::string_view name);

[[nodiscard]] bool is_replicable(core::FieldKind kind) noexcept;
[[nodiscard]] std::size_t max_wire_size(core::FieldKind kind) noexcept;
[[nodiscard]] bool fits_wire(const core::FieldValue& value) noexcept;
[[nodiscard]] std::size_t wire_size(const core::FieldValue& value) noexcept;
[[nodiscard]] std::size_t wire_size(const EntityState& entity) noexcept;

} // namespace engine::network
