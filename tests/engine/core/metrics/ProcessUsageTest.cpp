#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

#include "engine/core/metrics/Metrics.hpp"
#include "engine/core/metrics/ProcessUsage.hpp"
#include "engine/core/time/ManualClock.hpp"

using namespace std::chrono_literals;
using engine::core::metrics::Metrics;
using engine::core::metrics::ProcessMonitor;
using engine::core::metrics::ProcessSample;
using engine::core::metrics::sample_process;
using engine::core::time::ManualClock;

namespace {

constexpr std::uint64_t mebibyte = 1024ULL * 1024ULL;

class FakeProcess {
public:
    ProcessMonitor::Sampler sampler() {
        return [this] {
            ++samples;
            return current;
        };
    }

    ProcessSample current;
    int samples{0};
};

} // namespace

TEST(ProcessUsage, SampleProcessReportsThisProcess) {
    const ProcessSample sample = sample_process();

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    ASSERT_TRUE(sample.cpu_time.has_value());
    ASSERT_TRUE(sample.resident_bytes.has_value());
    EXPECT_GE(*sample.cpu_time, 0ns);
    EXPECT_GT(*sample.resident_bytes, 0U);
#endif
}

TEST(ProcessMonitor, RejectsAnEmptySamplerOrANonPositivePeriod) {
    const ManualClock clock;
    Metrics metrics(clock);

    EXPECT_THROW(ProcessMonitor(metrics, {}), std::invalid_argument);
    EXPECT_THROW(ProcessMonitor(metrics, sample_process, 0ms), std::invalid_argument);
}

TEST(ProcessMonitor, PublishesMemoryInMebibytes) {
    const ManualClock clock;
    Metrics metrics(clock);
    FakeProcess process;
    process.current.resident_bytes = 48 * mebibyte;
    ProcessMonitor monitor(metrics, process.sampler());

    monitor.update();
    metrics.end_frame();

    EXPECT_DOUBLE_EQ(metrics.summary("process.memory")->last, 48.0);
}

TEST(ProcessMonitor, PublishesCpuAsAPercentOfOneCoreFromTwoSamples) {
    ManualClock clock;
    Metrics metrics(clock);
    FakeProcess process;
    process.current.cpu_time = 1s;
    ProcessMonitor monitor(metrics, process.sampler(), 500ms);

    monitor.update();
    EXPECT_FALSE(metrics.summary("process.cpu").has_value());

    clock.advance(500ms);
    process.current.cpu_time = 1s + 250ms;
    monitor.update();
    metrics.end_frame();

    EXPECT_DOUBLE_EQ(metrics.summary("process.cpu")->last, 50.0);
}

TEST(ProcessMonitor, SamplesAtMostOncePerPeriod) {
    ManualClock clock;
    Metrics metrics(clock);
    FakeProcess process;
    ProcessMonitor monitor(metrics, process.sampler(), 500ms);

    monitor.update();
    clock.advance(499ms);
    monitor.update();
    clock.advance(1ms);
    monitor.update();

    EXPECT_EQ(process.samples, 2);
}

TEST(ProcessMonitor, LeavesGaugesUntouchedWhenTheSystemCannotTell) {
    ManualClock clock;
    Metrics metrics(clock);
    FakeProcess process;
    ProcessMonitor monitor(metrics, process.sampler(), 500ms);

    monitor.update();
    clock.advance(1s);
    monitor.update();

    EXPECT_TRUE(metrics.names().empty());
}

TEST(ProcessMonitor, ACpuTimeGoingBackwardsReadsZero) {
    ManualClock clock;
    Metrics metrics(clock);
    FakeProcess process;
    process.current.cpu_time = 2s;
    ProcessMonitor monitor(metrics, process.sampler(), 500ms);

    monitor.update();
    clock.advance(1s);
    process.current.cpu_time = 1s;
    monitor.update();
    metrics.end_frame();

    EXPECT_DOUBLE_EQ(metrics.summary("process.cpu")->last, 0.0);
}

TEST(ProcessMonitor, DoesNotSampleWhileMetricsAreDisabled) {
    const ManualClock clock;
    Metrics metrics(clock);
    metrics.set_enabled(false);
    FakeProcess process;
    ProcessMonitor monitor(metrics, process.sampler());

    monitor.update();

    EXPECT_EQ(process.samples, 0);
}
