#include "platform/Executable.hpp"

namespace editor {

std::string executable_name(std::string_view base) {
#if defined(_WIN32)
    return std::string(base) + ".exe";
#else
    return std::string(base);
#endif
}

} // namespace editor
