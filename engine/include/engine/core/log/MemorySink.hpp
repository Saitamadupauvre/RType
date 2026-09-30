#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "engine/core/Export.hpp"
#include "engine/core/log/Level.hpp"
#include "engine/core/log/Sink.hpp"

namespace engine::core::log {

/**
 * @brief A record owned by a MemorySink.
 */
struct MemoryRecord {
    Level level;
    std::string message;
};

/**
 * @brief Sink that keeps every record in memory, meant for tests.
 *
 * @note Thread safe: write, records and clear may be called concurrently.
 */
class ENGINE_CORE_EXPORT MemorySink final : public Sink {
public:
    MemorySink();
    ~MemorySink() override;

    /**
     * @brief Stores a copy of the record.
     *
     * @param record The message and its level.
     */
    void write(const Record& record) override;

    /**
     * @brief Returns a snapshot of the stored records, oldest first.
     *
     * @return A copy of the records, safe to inspect while other threads keep logging.
     */
    [[nodiscard]] std::vector<MemoryRecord> records() const;

    /**
     * @brief Removes every stored record.
     */
    void clear();

private:
    mutable std::mutex mutex_;
    std::vector<MemoryRecord> records_;
};

} // namespace engine::core::log
