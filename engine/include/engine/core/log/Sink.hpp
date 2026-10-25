#pragma once

#include <string_view>

#include "engine/core/Export.hpp"
#include "engine/core/log/Level.hpp"

namespace engine::core::log {

/**
 * @brief One log message handed to a sink.
 *
 * @note The message view is only valid during the call to Sink::write.
 */
struct Record {
    Level level;
    std::string_view message;
};

/**
 * @brief Destination of log messages.
 *
 * @note Implementations must tolerate concurrent calls to write from several threads.
 */
class ENGINE_CORE_EXPORT Sink {
public:
    Sink() = default;
    Sink(const Sink&) = delete;
    Sink& operator=(const Sink&) = delete;
    Sink(Sink&&) = delete;
    Sink& operator=(Sink&&) = delete;
    virtual ~Sink() = default;

    /**
     * @brief Consumes one record.
     *
     * @param record The message and its level.
     * @note Must not throw; the logger swallows exceptions from sinks.
     */
    virtual void write(const Record& record) = 0;
};

} // namespace engine::core::log
