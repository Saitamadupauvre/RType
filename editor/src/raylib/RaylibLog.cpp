#include "raylib/RaylibLog.hpp"

#include <raylib.h>

#include <array>
#include <cstdarg>
#include <cstdio>
#include <string_view>

#include "engine/core/log/Log.hpp"

namespace editor {

namespace {

constexpr std::size_t message_size = 1024;

void forward(int level, const char* format, va_list arguments) {
    namespace logging = engine::core::log;
    std::array<char, message_size> message{};
    std::vsnprintf(message.data(), message.size(), format, arguments);
    const std::string_view text(message.data());
    if (level >= LOG_ERROR) {
        logging::error("raylib: {}", text);
    } else if (level == LOG_WARNING) {
        logging::warn("raylib: {}", text);
    } else if (level == LOG_INFO) {
        logging::debug("raylib: {}", text);
    } else {
        logging::trace("raylib: {}", text);
    }
}

} // namespace

void route_raylib_log() {
    SetTraceLogLevel(LOG_ALL);
    SetTraceLogCallback(forward);
}

} // namespace editor
