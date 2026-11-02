#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "bytes/ByteReader.hpp"
#include "protocol/Messages.hpp"

namespace engine::network {

[[nodiscard]] std::optional<std::string> read_text(ByteReader& reader);
[[nodiscard]] std::optional<WireValue> read_value(ByteReader& reader, std::uint8_t kind);

} // namespace engine::network
