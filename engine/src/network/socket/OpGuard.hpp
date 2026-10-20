#pragma once

#include <atomic>

namespace engine::network::detail {

class OpGuard {
public:
    OpGuard(const std::atomic<int>& handle, std::atomic<unsigned int>& active_ops) noexcept
        : handle_(handle), active_ops_(active_ops) {
        active_ops_.fetch_add(1, std::memory_order_seq_cst);
        fd = handle_.load(std::memory_order_seq_cst);
    }

    ~OpGuard() {
        if (active_ops_.fetch_sub(1, std::memory_order_seq_cst) == 1) {
            active_ops_.notify_all();
        }
    }

    OpGuard(const OpGuard&) = delete;
    OpGuard& operator=(const OpGuard&) = delete;
    OpGuard(OpGuard&&) = delete;
    OpGuard& operator=(OpGuard&&) = delete;

    [[nodiscard]] bool valid() const noexcept { return fd >= 0; }

    [[nodiscard]] bool closing() const noexcept {
        return handle_.load(std::memory_order_acquire) != fd;
    }

    int fd;

private:
    const std::atomic<int>& handle_;
    std::atomic<unsigned int>& active_ops_;
};

} // namespace engine::network::detail
