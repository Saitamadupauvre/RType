#pragma once

#include <atomic>

namespace engine::network::detail {

/// RAII "in-flight operation" pin for a resource handle (fd) that another thread may close.
///
/// Protocol (shared with the closer):
///   guard : ++active, THEN read handle
///   closer: handle = -1, THEN wait until active == 0, THEN release the resource
/// With seq_cst ordering, either the guard sees -1 and bails out, or the closer sees
/// active > 0 and waits. The OS can therefore never recycle the fd while we still use it.
///
/// Internal helper: not part of the public API.
class OpGuard {
public:
    OpGuard(const std::atomic<int>& handle, std::atomic<unsigned int>& active) noexcept
        : _handle(handle), _active(active) {
        _active.fetch_add(1, std::memory_order_seq_cst);
        fd = _handle.load(std::memory_order_seq_cst);
    }

    ~OpGuard() {
        if (_active.fetch_sub(1, std::memory_order_seq_cst) == 1)
            _active.notify_all(); // wake a close() waiting for us
    }

    OpGuard(const OpGuard&) = delete;
    OpGuard& operator=(const OpGuard&) = delete;
    OpGuard(OpGuard&&) = delete;
    OpGuard& operator=(OpGuard&&) = delete;

    /// False if the socket was already closed when the operation started.
    [[nodiscard]] bool valid() const noexcept { return fd >= 0; }

    /// True if close() was requested after this operation started.
    [[nodiscard]] bool closing() const noexcept {
        return _handle.load(std::memory_order_acquire) != fd;
    }

    int fd; // snapshot taken at construction; stays valid until the guard dies

private:
    const std::atomic<int>& _handle;
    std::atomic<unsigned int>& _active;
};

} // namespace engine::network::detail
