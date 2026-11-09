#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "engine/core/Export.hpp"
#include "engine/core/time/IClock.hpp"

namespace engine::core::metrics {

/** @brief How the samples of a metric are produced within one frame. */
enum class MetricKind : std::uint8_t {
    /** @brief Durations in milliseconds, summed over the frame; 0 for a frame that recorded none.
     */
    timer,
    /** @brief Amounts summed over the frame; 0 for a frame that counted none. */
    counter,
    /** @brief The last value set; kept from frame to frame until set again. */
    gauge,
};

/** @brief Statistics of one metric over the frames kept in its history. */
struct MetricSummary {
    MetricKind kind{MetricKind::gauge};
    /** @brief Sample of the latest completed frame. */
    double last{};
    double average{};
    double max{};
    /** @brief Number of frames kept, at most Metrics::capacity(). */
    std::size_t samples{};
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/**
 * @brief Collects named timers, counters and gauges, one sample per frame, into a fixed-size
 * history per metric.
 *
 * Writers record into the current frame; end_frame() turns every metric known so far into one
 * sample of its history, the oldest sample being overwritten once the history is full. Readers
 * only see completed frames.
 *
 * A metric takes the kind of its first write. A later write of another kind, an empty name or a
 * non-finite value is dropped and logged once per name.
 *
 * While disabled every write and end_frame() returns at once without taking the lock, and a
 * ScopedTimer does not read the clock; the history recorded before is kept.
 *
 * @note Thread safe: any number of threads may write and read at the same time, for instance the
 * server thread writing while the client thread draws the values in host mode.
 */
class ENGINE_CORE_EXPORT Metrics {
public:
    /** @brief Frames kept by default: two seconds at 60 frames per second. */
    static constexpr std::size_t default_capacity = 120;

    /**
     * @param clock Time source of ScopedTimer; must outlive this object.
     * @param capacity Frames kept per metric.
     * @throws std::invalid_argument If capacity is zero.
     */
    explicit Metrics(const time::IClock& clock, std::size_t capacity = default_capacity);
    ~Metrics();

    Metrics(const Metrics&) = delete;
    Metrics& operator=(const Metrics&) = delete;
    Metrics(Metrics&&) = delete;
    Metrics& operator=(Metrics&&) = delete;

    /** @brief Turns collection on or off. Metrics start enabled. */
    void set_enabled(bool enabled) noexcept;
    [[nodiscard]] bool enabled() const noexcept;

    /** @brief Adds @p elapsed to the timer @p name for the current frame. */
    void record_time(std::string_view name, time::Duration elapsed) noexcept;

    /** @brief Adds @p amount to the counter @p name for the current frame. */
    void count(std::string_view name, double amount = 1.0) noexcept;

    /** @brief Sets the gauge @p name to @p value. */
    void set_gauge(std::string_view name, double value) noexcept;

    /** @brief Closes the current frame: appends one sample to every known metric. */
    void end_frame() noexcept;

    /** @return The statistics of @p name, or nothing if it was never written. */
    [[nodiscard]] std::optional<MetricSummary> summary(std::string_view name) const;

    /** @return The samples of @p name from the oldest to the latest frame; empty if unknown. */
    [[nodiscard]] std::vector<double> history(std::string_view name) const;

    /** @return The name of every metric written so far, sorted. */
    [[nodiscard]] std::vector<std::string> names() const;

    [[nodiscard]] std::size_t capacity() const noexcept;

    /** @return The clock given to the constructor. */
    [[nodiscard]] const time::IClock& clock() const noexcept;

private:
    struct Slot {
        MetricKind kind;
        double pending{};
        std::vector<double> samples;
        std::size_t next{};
        std::size_t filled{};
    };

    void write(std::string_view name, MetricKind kind, double value);
    void warn_once(std::string_view name, std::string_view reason);
    void push(Slot& slot, double sample) const;
    [[nodiscard]] std::vector<double> ordered(const Slot& slot) const;

    std::reference_wrapper<const time::IClock> _clock;
    std::size_t _capacity;
    std::atomic<bool> _enabled{true};
    mutable std::mutex _mutex;
    std::map<std::string, Slot, std::less<>> _slots;
    std::set<std::string, std::less<>> _warned;
};

/**
 * @brief Measures the time between its construction and its destruction and adds it to a timer.
 *
 * @code
 * {
 *     const ScopedTimer timer{metrics, "lua.update"};
 *     runtime.update(dt);
 * }
 * @endcode
 *
 * @note @p name must outlive the timer; a string literal is the usual choice. Nothing is read or
 * recorded if the metrics were disabled when the timer was created.
 */
class ENGINE_CORE_EXPORT ScopedTimer {
public:
    ScopedTimer(Metrics& metrics, std::string_view name) noexcept;
    ~ScopedTimer();

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
    ScopedTimer(ScopedTimer&&) = delete;
    ScopedTimer& operator=(ScopedTimer&&) = delete;

private:
    std::reference_wrapper<Metrics> _metrics;
    std::string_view _name;
    std::optional<time::Duration> _start;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace engine::core::metrics
