#pragma once

#include <array>
#include <string_view>

#include "loading/Declarations.hpp"

namespace engine::script {

inline constexpr std::array<std::string_view, 2> section_names{"server", "client"};

bool apply_section(const Declaration& declaration, std::string_view section);

} // namespace engine::script
