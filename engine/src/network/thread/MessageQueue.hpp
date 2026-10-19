#pragma once

#include <condition_variable>
#include <cstddef>
#include <limits>
#include <mutex>
#include <stop_token>
#include <utility>
#include <vector>

namespace engine::network {

template <typename T> class MessageQueue {
public:
    MessageQueue() = default;
    explicit MessageQueue(std::size_t capacity) : capacity_(capacity) {}

    bool push(T value) {
        {
            const std::scoped_lock lock(mutex_);
            if (items_.size() >= capacity_) {
                return false;
            }
            items_.push_back(std::move(value));
        }
        ready_.notify_one();
        return true;
    }

    [[nodiscard]] std::vector<T> drain() {
        std::vector<T> drained;
        const std::scoped_lock lock(mutex_);
        drained.swap(items_);
        return drained;
    }

    [[nodiscard]] std::vector<T> wait_and_drain(std::stop_token stop_token) {
        std::vector<T> drained;
        std::unique_lock lock(mutex_);
        ready_.wait(lock, stop_token, [this] { return !items_.empty(); });
        drained.swap(items_);
        return drained;
    }

private:
    std::size_t capacity_{std::numeric_limits<std::size_t>::max()};
    std::mutex mutex_;
    std::condition_variable_any ready_;
    std::vector<T> items_;
};

} // namespace engine::network
