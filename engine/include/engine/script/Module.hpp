#pragma once

#include <string_view>

#include "engine/script/Export.hpp"

namespace engine::script {

/**
 * @brief Returns the name of the script module shared library.
 *
 * @return The library name, "engine-script".
 */
ENGINE_SCRIPT_EXPORT std::string_view module_name() noexcept;

} // namespace engine::script
