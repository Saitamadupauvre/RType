#include "engine/core/console/LogCapture.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace engine::core::console {

LogCapture::LogCapture(std::shared_ptr<log::Sink> next, std::size_t max_pending)
    : _next(std::move(next)), _max_pending(std::max<std::size_t>(max_pending, 1)) {}

namespace {

void forward(log::Sink* next, const log::Record& record) noexcept {
    if (next == nullptr) {
        return;
    }
    try {
        next->write(record);
    } catch (...) {
        return;
    }
}

} // namespace

void LogCapture::write(const log::Record& record) {
    forward(_next.get(), record);
    try {
        const std::scoped_lock lock(_mutex);
        if (_pending.size() == _max_pending) {
            _pending.erase(_pending.begin());
        }
        _pending.push_back({.level = record.level, .message = std::string(record.message)});
    } catch (...) {
        return;
    }
}

std::vector<log::MemoryRecord> LogCapture::take() {
    const std::scoped_lock lock(_mutex);
    return std::exchange(_pending, {});
}

const std::shared_ptr<log::Sink>& LogCapture::next() const noexcept { return _next; }

} // namespace engine::core::console
