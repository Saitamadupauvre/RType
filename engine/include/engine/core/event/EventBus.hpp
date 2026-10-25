#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <typeindex>
#include <utility>

#include "engine/core/Export.hpp"
#include "engine/core/event/Subscription.hpp"

namespace engine::core::event {

/**
 * @brief Returns the process wide numeric id of an event type.
 *
 * @param type The dynamic type of the event.
 * @return The same id for the same type on every call, whichever module asks.
 * @note Thread safe.
 */
[[nodiscard]] ENGINE_CORE_EXPORT std::size_t event_type_id(std::type_index type);

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/**
 * @brief Typed publish/subscribe channel letting subsystems talk without knowing each other.
 *
 * Events are delivered immediately with publish, or kept with enqueue until dispatch is called.
 * A handler only receives the event type it subscribed to. Handlers of one type run in
 * subscription order. An exception thrown by a handler is caught and logged, and the remaining
 * handlers still run.
 *
 * Handlers may subscribe, unsubscribe (themselves or others) and enqueue while an event is
 * being delivered:
 * - a handler subscribed during delivery does not receive the event being delivered;
 * - a handler unsubscribed during delivery is not called afterwards, even if it was pending;
 * - an event enqueued during dispatch is kept for the next dispatch.
 *
 * @note Event types must be copy or move constructible for enqueue. enqueue is thread safe;
 * every other member function must be called from a single thread at a time.
 */
class ENGINE_CORE_EXPORT EventBus {
public:
    EventBus();
    ~EventBus();

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&) = delete;
    EventBus& operator=(EventBus&&) = delete;

    /**
     * @brief Registers a handler for events of type E.
     *
     * @tparam E The event type; must be given explicitly.
     * @param handler Called with every published or dispatched E.
     * @return A handle to pass to unsubscribe.
     */
    template <typename E> Subscription subscribe(std::function<void(const E&)> handler) {
        return add_handler(event_type_id(typeid(E)),
                           [callback = std::move(handler)](const void* event) {
                               callback(*static_cast<const E*>(event));
                           });
    }

    /**
     * @brief Removes a handler.
     *
     * @param subscription Handle returned by subscribe; an unknown or already removed handle is
     * ignored.
     */
    void unsubscribe(Subscription subscription);

    /**
     * @brief Delivers an event to the handlers of its type right now.
     *
     * @tparam E The event type, deduced.
     * @param event The event, only borrowed for the duration of the call.
     */
    template <typename E> void publish(const E& event) {
        publish_erased(event_type_id(typeid(E)), &event);
    }

    /**
     * @brief Keeps an event until the next dispatch.
     *
     * @tparam E The event type, deduced.
     * @param event The event, moved into the queue.
     * @note Thread safe.
     */
    template <typename E> void enqueue(E event) {
        enqueue_erased(event_type_id(typeid(E)), std::make_shared<E>(std::move(event)));
    }

    /**
     * @brief Delivers every queued event, oldest first, then empties the queue.
     *
     * @note Events enqueued by handlers during this call wait for the next dispatch.
     */
    void dispatch();

private:
    using ErasedHandler = std::function<void(const void*)>;

    Subscription add_handler(std::size_t type, ErasedHandler handler);
    void publish_erased(std::size_t type, const void* event);
    void enqueue_erased(std::size_t type, std::shared_ptr<const void> event);

    struct Impl;
    std::unique_ptr<Impl> _impl;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace engine::core::event
