#include "engine/core/metrics/Metrics.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <numeric>
#include <stdexcept>

#include "engine/core/log/Log.hpp"

namespace engine::core::metrics {

namespace {

double to_milliseconds(time::Duration elapsed) {
    return std::chrono::duration<double, std::milli>(elapsed).count();
}

std::string_view kind_name(MetricKind kind) {
    switch (kind) {
    case MetricKind::timer:
        return "timer";
    case MetricKind::counter:
        return "counter";
    case MetricKind::gauge:
        return "gauge";
    }
    return "metric";
}

} // namespace

Metrics::Metrics(const time::IClock& clock, std::size_t capacity)
    : _clock(clock), _capacity(capacity) {
    if (capacity == 0) {
        throw std::invalid_argument("Metrics capacity must be positive");
    }
}

Metrics::~Metrics() = default;

void Metrics::set_enabled(bool enabled) noexcept { _enabled.store(enabled); }

bool Metrics::enabled() const noexcept { return _enabled.load(); }

void Metrics::record_time(std::string_view name, time::Duration elapsed) noexcept {
    if (!enabled()) {
        return;
    }
    try {
        write(name, MetricKind::timer, to_milliseconds(elapsed));
    } catch (...) {
        log::error("metrics: cannot record timer '{}'", name);
    }
}

void Metrics::count(std::string_view name, double amount) noexcept {
    if (!enabled()) {
        return;
    }
    try {
        write(name, MetricKind::counter, amount);
    } catch (...) {
        log::error("metrics: cannot record counter '{}'", name);
    }
}

void Metrics::set_gauge(std::string_view name, double value) noexcept {
    if (!enabled()) {
        return;
    }
    try {
        write(name, MetricKind::gauge, value);
    } catch (...) {
        log::error("metrics: cannot record gauge '{}'", name);
    }
}

void Metrics::write(std::string_view name, MetricKind kind, double value) {
    const std::scoped_lock lock(_mutex);
    if (name.empty()) {
        warn_once(name, "a metric needs a name");
        return;
    }
    if (!std::isfinite(value)) {
        warn_once(name, "non-finite values are dropped");
        return;
    }
    auto found = _slots.find(name);
    if (found == _slots.end()) {
        found = _slots
                    .emplace(std::string(name),
                             Slot{.kind = kind, .samples = std::vector<double>(_capacity)})
                    .first;
    }
    Slot& slot = found->second;
    if (slot.kind != kind) {
        warn_once(name, std::format("it is a {}", kind_name(slot.kind)));
        return;
    }
    if (kind == MetricKind::gauge) {
        slot.pending = value;
    } else {
        slot.pending += value;
    }
}

void Metrics::warn_once(std::string_view name, std::string_view reason) {
    if (_warned.contains(name)) {
        return;
    }
    _warned.emplace(name);
    log::warn("metrics: dropped a write to '{}': {}", name, reason);
}

void Metrics::end_frame() noexcept {
    if (!enabled()) {
        return;
    }
    const std::scoped_lock lock(_mutex);
    for (auto& [name, slot] : _slots) {
        push(slot, slot.pending);
        if (slot.kind != MetricKind::gauge) {
            slot.pending = 0.0;
        }
    }
}

void Metrics::push(Slot& slot, double sample) const {
    slot.samples[slot.next] = sample;
    slot.next = (slot.next + 1) % _capacity;
    slot.filled = std::min(slot.filled + 1, _capacity);
}

std::vector<double> Metrics::ordered(const Slot& slot) const {
    std::vector<double> samples;
    samples.reserve(slot.filled);
    const std::size_t oldest = (slot.next + _capacity - slot.filled) % _capacity;
    for (std::size_t offset = 0; offset < slot.filled; ++offset) {
        samples.push_back(slot.samples[(oldest + offset) % _capacity]);
    }
    return samples;
}

std::optional<MetricSummary> Metrics::summary(std::string_view name) const {
    const std::scoped_lock lock(_mutex);
    const auto found = _slots.find(name);
    if (found == _slots.end()) {
        return std::nullopt;
    }
    const Slot& slot = found->second;
    MetricSummary summary{.kind = slot.kind, .samples = slot.filled};
    if (slot.filled == 0) {
        return summary;
    }
    const auto samples = ordered(slot);
    summary.last = samples.back();
    summary.max = std::ranges::max(samples);
    summary.average =
        std::accumulate(samples.begin(), samples.end(), 0.0) / static_cast<double>(samples.size());
    return summary;
}

std::vector<double> Metrics::history(std::string_view name) const {
    const std::scoped_lock lock(_mutex);
    const auto found = _slots.find(name);
    if (found == _slots.end()) {
        return {};
    }
    return ordered(found->second);
}

std::vector<std::string> Metrics::names() const {
    const std::scoped_lock lock(_mutex);
    std::vector<std::string> names;
    names.reserve(_slots.size());
    for (const auto& [name, slot] : _slots) {
        names.push_back(name);
    }
    return names;
}

std::size_t Metrics::capacity() const noexcept { return _capacity; }

const time::IClock& Metrics::clock() const noexcept { return _clock.get(); }

ScopedTimer::ScopedTimer(Metrics& metrics, std::string_view name) noexcept
    : _metrics(metrics), _name(name) {
    if (metrics.enabled()) {
        _start = metrics.clock().now();
    }
}

ScopedTimer::~ScopedTimer() {
    if (!_start) {
        return;
    }
    const time::Duration elapsed = _metrics.get().clock().now() - *_start;
    _metrics.get().record_time(_name, std::max(elapsed, time::Duration::zero()));
}

} // namespace engine::core::metrics
