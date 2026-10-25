#pragma once

#include <format>
#include <memory>
#include <string_view>
#include <utility>

#include "engine/core/Export.hpp"
#include "engine/core/log/Level.hpp"
#include "engine/core/log/Sink.hpp"

namespace engine::core::log {

/**
 * @brief Sets the minimum level a message needs to reach the sink.
 *
 * @param threshold Messages below it are dropped; Level::Off drops everything.
 * @note Thread safe.
 */
ENGINE_CORE_EXPORT void set_level(Level threshold) noexcept;

/**
 * @brief Returns the current minimum level. The default is Level::Info.
 *
 * @note Thread safe.
 */
[[nodiscard]] ENGINE_CORE_EXPORT Level level() noexcept;

/**
 * @brief Replaces the destination of log messages.
 *
 * @param sink The new sink; a null pointer restores the default stderr sink.
 * @note Thread safe. Messages already being written finish on the previous sink.
 */
ENGINE_CORE_EXPORT void set_sink(std::shared_ptr<Sink> sink);

/**
 * @brief Returns the sink currently receiving messages.
 *
 * @return A non-null sink.
 * @note Thread safe.
 */
[[nodiscard]] ENGINE_CORE_EXPORT std::shared_ptr<Sink> current_sink();

/**
 * @brief Sends an already formatted message to the sink if its level passes the threshold.
 *
 * @param message_level Severity of the message.
 * @param message Text sent as is, without any formatting.
 * @note Thread safe. Never throws: an exception raised by the sink is swallowed.
 */
ENGINE_CORE_EXPORT void write(Level message_level, std::string_view message) noexcept;

namespace detail {

template <typename... Args>
void format_and_write(Level message_level, std::format_string<Args...> format,
                      Args&&... args) noexcept {
    if (message_level < level()) {
        return;
    }
    try {
        write(message_level, std::format(format, std::forward<Args>(args)...));
    } catch (...) {
        write(Level::Error, "log message formatting failed");
    }
}

} // namespace detail

/**
 * @brief Formats and logs a message at Level::Trace.
 *
 * @note Formatting is skipped when the level is filtered out. Never throws.
 */
template <typename... Args>
void trace(std::format_string<Args...> format, Args&&... args) noexcept {
    detail::format_and_write(Level::Trace, format, std::forward<Args>(args)...);
}

/**
 * @brief Formats and logs a message at Level::Debug.
 *
 * @note Formatting is skipped when the level is filtered out. Never throws.
 */
template <typename... Args>
void debug(std::format_string<Args...> format, Args&&... args) noexcept {
    detail::format_and_write(Level::Debug, format, std::forward<Args>(args)...);
}

/**
 * @brief Formats and logs a message at Level::Info.
 *
 * @note Formatting is skipped when the level is filtered out. Never throws.
 */
template <typename... Args> void info(std::format_string<Args...> format, Args&&... args) noexcept {
    detail::format_and_write(Level::Info, format, std::forward<Args>(args)...);
}

/**
 * @brief Formats and logs a message at Level::Warn.
 *
 * @note Formatting is skipped when the level is filtered out. Never throws.
 */
template <typename... Args> void warn(std::format_string<Args...> format, Args&&... args) noexcept {
    detail::format_and_write(Level::Warn, format, std::forward<Args>(args)...);
}

/**
 * @brief Formats and logs a message at Level::Error.
 *
 * @note Formatting is skipped when the level is filtered out. Never throws.
 */
template <typename... Args>
void error(std::format_string<Args...> format, Args&&... args) noexcept {
    detail::format_and_write(Level::Error, format, std::forward<Args>(args)...);
}

/**
 * @brief Installs a sink and a level for the lifetime of the object, then restores the previous
 * ones.
 *
 * @note Intended for tests. Not thread safe against concurrent ScopedSink objects.
 */
class ScopedSink {
public:
    ScopedSink(std::shared_ptr<Sink> sink, Level threshold)
        : previous_sink_(current_sink()), previous_level_(level()) {
        set_sink(std::move(sink));
        set_level(threshold);
    }

    ScopedSink(const ScopedSink&) = delete;
    ScopedSink& operator=(const ScopedSink&) = delete;
    ScopedSink(ScopedSink&&) = delete;
    ScopedSink& operator=(ScopedSink&&) = delete;

    ~ScopedSink() {
        set_sink(std::move(previous_sink_));
        set_level(previous_level_);
    }

private:
    std::shared_ptr<Sink> previous_sink_;
    Level previous_level_;
};

} // namespace engine::core::log
