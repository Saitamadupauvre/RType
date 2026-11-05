#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

#include "engine/core/Export.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/core/log/Sink.hpp"

namespace engine::core::console {

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/**
 * @brief Log sink that forwards every record to another sink and keeps a copy for the console.
 *
 * Install it with log::set_sink(), then drain the copies with take() from the thread that owns
 * the console.
 *
 * @note Thread safe: write() may run on any thread while take() runs on another.
 */
class ENGINE_CORE_EXPORT LogCapture final : public log::Sink {
public:
    /** @brief Default number of records kept between two calls to take(). */
    static constexpr std::size_t default_max_pending = 1024;

    /**
     * @param next Sink receiving every record too; may be null.
     * @param max_pending Records kept until take(); the oldest are dropped beyond it.
     */
    explicit LogCapture(std::shared_ptr<log::Sink> next,
                        std::size_t max_pending = default_max_pending);

    /** @brief Forwards the record to the next sink and keeps a copy. Never throws. */
    void write(const log::Record& record) override;

    /** @return The records written since the last call, oldest first. */
    [[nodiscard]] std::vector<log::MemoryRecord> take();

    /** @return The sink records are forwarded to. */
    [[nodiscard]] const std::shared_ptr<log::Sink>& next() const noexcept;

private:
    std::shared_ptr<log::Sink> _next;
    std::size_t _max_pending;
    std::mutex _mutex;
    std::vector<log::MemoryRecord> _pending;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace engine::core::console
