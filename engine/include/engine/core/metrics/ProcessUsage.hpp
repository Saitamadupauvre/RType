#pragma once

#include <cstdint>
#include <functional>
#include <optional>

#include "engine/core/Export.hpp"
#include "engine/core/metrics/Metrics.hpp"
#include "engine/core/time/IClock.hpp"

namespace engine::core::metrics {

/** @brief Resources used by the current process since it started. */
struct ProcessSample {
    /** @brief CPU time spent by every thread, user and system; empty if the system cannot tell. */
    std::optional<time::Duration> cpu_time;
    /** @brief Resident memory in bytes; empty if the system cannot tell. */
    std::optional<std::uint64_t> resident_bytes;
};

/**
 * @brief Reads the CPU time and resident memory of the current process from the system.
 *
 * @return The values the platform can provide (Linux, macOS, Windows); a field stays empty
 * elsewhere or when the system call fails.
 * @note Thread safe. Never throws.
 */
[[nodiscard]] ENGINE_CORE_EXPORT ProcessSample sample_process() noexcept;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/**
 * @brief Publishes the CPU and memory usage of the process as gauges, at most once per period.
 *
 * Sets `process.cpu` to the CPU time used since the previous sample divided by the elapsed time,
 * in percent of one core (a busy two-thread process reads 200), and `process.memory` to the
 * resident memory in MiB. A value the sampler cannot provide leaves its gauge untouched; the first
 * CPU value needs two samples.
 *
 * @note Not thread safe: call update() from one thread.
 */
class ENGINE_CORE_EXPORT ProcessMonitor {
public:
    using Sampler = std::function<ProcessSample()>;

    /** @brief Time between two samples by default. */
    static constexpr time::Duration default_period = std::chrono::milliseconds(500);

    /**
     * @param metrics Receives the gauges; its clock paces the samples. Must outlive the monitor.
     * @param sampler Source of the samples, sample_process() by default.
     * @param period Minimum time between two samples.
     * @throws std::invalid_argument If @p sampler is empty or @p period is not positive.
     */
    explicit ProcessMonitor(Metrics& metrics, Sampler sampler = sample_process,
                            time::Duration period = default_period);

    /** @brief Samples the process if a period passed since the previous sample. */
    void update();

private:
    std::reference_wrapper<Metrics> _metrics;
    Sampler _sampler;
    time::Duration _period;
    std::optional<time::Duration> _last_sample_at;
    std::optional<time::Duration> _last_cpu_time;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace engine::core::metrics
