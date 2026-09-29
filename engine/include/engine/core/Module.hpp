#pragma once

#include <string_view>

#include "engine/core/Export.hpp"

namespace engine::core {

/**
 * @brief Returns the name of the core module shared library.
 *
 * @return The library name, "engine-core".
 */
ENGINE_CORE_EXPORT std::string_view module_name() noexcept;

} // namespace engine::core
