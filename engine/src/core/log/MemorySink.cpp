#include "engine/core/log/MemorySink.hpp"

namespace engine::core::log {

MemorySink::MemorySink() = default;

MemorySink::~MemorySink() = default;

void MemorySink::write(const Record& record) {
    const std::scoped_lock lock(mutex_);
    records_.push_back(MemoryRecord{.level = record.level, .message = std::string(record.message)});
}

std::vector<MemoryRecord> MemorySink::records() const {
    const std::scoped_lock lock(mutex_);
    return records_;
}

void MemorySink::clear() {
    const std::scoped_lock lock(mutex_);
    records_.clear();
}

} // namespace engine::core::log
