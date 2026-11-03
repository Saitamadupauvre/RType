#include "engine/core/metrics/ProcessUsage.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <psapi.h>
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <sys/resource.h>
#elif defined(__linux__)
#include <fstream>
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace engine::core::metrics {

namespace {

constexpr double bytes_per_mebibyte = 1024.0 * 1024.0;

#if defined(_WIN32)

time::Duration from_filetime(const FILETIME& value) {
    ULARGE_INTEGER ticks{};
    ticks.LowPart = value.dwLowDateTime;
    ticks.HighPart = value.dwHighDateTime;
    constexpr std::uint64_t nanoseconds_per_tick = 100;
    return time::Duration(static_cast<std::int64_t>(ticks.QuadPart * nanoseconds_per_tick));
}

std::optional<time::Duration> cpu_time() noexcept {
    FILETIME created{};
    FILETIME exited{};
    FILETIME kernel{};
    FILETIME user{};
    if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user) == 0) {
        return std::nullopt;
    }
    return from_filetime(kernel) + from_filetime(user);
}

std::optional<std::uint64_t> resident_bytes() noexcept {
    PROCESS_MEMORY_COUNTERS counters{};
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)) == 0) {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(counters.WorkingSetSize);
}

#elif defined(__APPLE__) || defined(__linux__)

time::Duration from_timeval(const timeval& value) {
    return std::chrono::seconds(value.tv_sec) + std::chrono::microseconds(value.tv_usec);
}

std::optional<time::Duration> cpu_time() noexcept {
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return std::nullopt;
    }
    return from_timeval(usage.ru_utime) + from_timeval(usage.ru_stime);
}

#if defined(__APPLE__)

std::optional<std::uint64_t> resident_bytes() noexcept {
    mach_task_basic_info info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info),
                  &count) != KERN_SUCCESS) {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(info.resident_size);
}

#else

std::optional<std::uint64_t> resident_bytes() noexcept {
    try {
        std::ifstream statm("/proc/self/statm");
        std::uint64_t total_pages = 0;
        std::uint64_t resident_pages = 0;
        if (!(statm >> total_pages >> resident_pages)) {
            return std::nullopt;
        }
        const long page_size = sysconf(_SC_PAGESIZE);
        if (page_size <= 0) {
            return std::nullopt;
        }
        return resident_pages * static_cast<std::uint64_t>(page_size);
    } catch (...) {
        return std::nullopt;
    }
}

#endif

#else

std::optional<time::Duration> cpu_time() noexcept { return std::nullopt; }

std::optional<std::uint64_t> resident_bytes() noexcept { return std::nullopt; }

#endif

} // namespace

ProcessSample sample_process() noexcept {
    return ProcessSample{.cpu_time = cpu_time(), .resident_bytes = resident_bytes()};
}

ProcessMonitor::ProcessMonitor(Metrics& metrics, Sampler sampler, time::Duration period)
    : _metrics(metrics), _sampler(std::move(sampler)), _period(period) {
    if (!_sampler) {
        throw std::invalid_argument("ProcessMonitor needs a sampler");
    }
    if (_period <= time::Duration::zero()) {
        throw std::invalid_argument("ProcessMonitor period must be positive");
    }
}

void ProcessMonitor::update() {
    Metrics& metrics = _metrics.get();
    if (!metrics.enabled()) {
        return;
    }
    const time::Duration now = metrics.clock().now();
    if (_last_sample_at && now - *_last_sample_at < _period) {
        return;
    }
    const ProcessSample sample = _sampler();
    if (sample.resident_bytes) {
        metrics.set_gauge("process.memory",
                          static_cast<double>(*sample.resident_bytes) / bytes_per_mebibyte);
    }
    if (sample.cpu_time && _last_cpu_time && _last_sample_at && now > *_last_sample_at) {
        const auto used = std::chrono::duration<double>(*sample.cpu_time - *_last_cpu_time);
        const auto elapsed = std::chrono::duration<double>(now - *_last_sample_at);
        metrics.set_gauge("process.cpu", std::max(0.0, used / elapsed * 100.0));
    }
    _last_cpu_time = sample.cpu_time;
    _last_sample_at = now;
}

} // namespace engine::core::metrics
