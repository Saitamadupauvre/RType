#include "StderrSink.hpp"

#include <cstdio>
#include <format>
#include <string>

namespace engine::core::log {

namespace {

std::string_view level_name(Level level) noexcept {
    switch (level) {
    case Level::Trace:
        return "TRACE";
    case Level::Debug:
        return "DEBUG";
    case Level::Info:
        return "INFO ";
    case Level::Warn:
        return "WARN ";
    case Level::Error:
        return "ERROR";
    case Level::Off:
        break;
    }
    return "OFF  ";
}

} // namespace

void StderrSink::write(const Record& record) {
    const std::string line = std::format("[{}] {}\n", level_name(record.level), record.message);
    std::fwrite(line.data(), 1, line.size(), stderr);
}

} // namespace engine::core::log
