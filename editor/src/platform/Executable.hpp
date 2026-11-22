#pragma once

#include <string>
#include <string_view>

namespace editor {

[[nodiscard]] std::string executable_name(std::string_view base);

} // namespace editor
