#pragma once

#include <string_view>

#include "engine/network/Export.hpp"

namespace engine::network {

/**
 * @brief Returns the name of the network module shared library.
 *
 * @return The library name, "engine-network".
 */
ENGINE_NETWORK_EXPORT std::string_view module_name() noexcept;

} // namespace engine::network
