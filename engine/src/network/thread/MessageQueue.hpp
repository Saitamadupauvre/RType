#pragma once

#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <utility>
#include <vector>

namespace engine::network {

template <typename T> class MessageQueue {
public:
    void push(T value) {
        {
            const std::scoped_lock lock(mutex_);
            items_.push_back(std::move(value));
        }
        ready_.notify_one();
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
    std::mutex mutex_;
    std::condition_variable_any ready_;
    std::vector<T> items_;
};

} // namespace engine::network
