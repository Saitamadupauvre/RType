#pragma once

#include <cstdint>

namespace engine::core::log {

/**
 * @brief Severity of a log message, in increasing order.
 *
 * @note `Off` is only meaningful as a threshold: it silences every message.
 */
enum class Level : std::uint8_t { Trace, Debug, Info, Warn, Error, Off };

} // namespace engine::core::log
