#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "bytes/ByteReader.hpp"
#include "engine/core/FieldRegistry.hpp"

namespace engine::network {

[[nodiscard]] std::optional<std::string> read_text(ByteReader& reader);
[[nodiscard]] std::optional<core::FieldValue> read_value(ByteReader& reader, std::uint8_t kind);

} // namespace engine::network
