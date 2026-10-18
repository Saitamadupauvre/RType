#pragma once

#include <mutex>
#include <utility>
#include <vector>

namespace engine::network {

template <typename T> class MessageQueue {
public:
    void push(T value) {
        const std::scoped_lock lock(mutex_);
        items_.push_back(std::move(value));
    }

    [[nodiscard]] std::vector<T> drain() {
        std::vector<T> drained;
        const std::scoped_lock lock(mutex_);
        drained.swap(items_);
        return drained;
    }

private:
    std::mutex mutex_;
    std::vector<T> items_;
};

} // namespace engine::network
