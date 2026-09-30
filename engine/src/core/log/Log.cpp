#include "engine/core/log/Log.hpp"

#include <atomic>
#include <mutex>

#include "StderrSink.hpp"

namespace engine::core::log {

namespace {

std::shared_ptr<Sink> make_default_sink() { return std::make_shared<StderrSink>(); }

struct State {
    std::atomic<Level> threshold{Level::Info};
    std::mutex mutex;
    std::shared_ptr<Sink> sink{make_default_sink()};
};

State& state() {
    static State instance;
    return instance;
}

} // namespace

void set_level(Level threshold) noexcept {
    state().threshold.store(threshold, std::memory_order_relaxed);
}

Level level() noexcept { return state().threshold.load(std::memory_order_relaxed); }

void set_sink(std::shared_ptr<Sink> sink) {
    if (!sink) {
        sink = make_default_sink();
    }
    State& current = state();
    const std::scoped_lock lock(current.mutex);
    current.sink = std::move(sink);
}

std::shared_ptr<Sink> current_sink() {
    State& current = state();
    const std::scoped_lock lock(current.mutex);
    return current.sink;
}

void write(Level message_level, std::string_view message) noexcept {
    if (message_level == Level::Off || message_level < level()) {
        return;
    }
    try {
        current_sink()->write(Record{.level = message_level, .message = message});
    } catch (...) {
        return;
    }
}

} // namespace engine::core::log
