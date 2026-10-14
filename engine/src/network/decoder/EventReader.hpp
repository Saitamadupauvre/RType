#pragma once

#include <cstdint>
#include <optional>

#include "bytes/ByteReader.hpp"
#include "protocol/Messages.hpp"

namespace engine::network {

[[nodiscard]] std::optional<EventPayload> read_event(ByteReader& reader,
                                                     std::uint8_t type) noexcept;

} // namespace engine::network
