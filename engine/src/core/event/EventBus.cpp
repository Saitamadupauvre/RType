#include "engine/core/event/EventBus.hpp"

#include <algorithm>
#include <exception>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "engine/core/log/Log.hpp"

namespace engine::core::event {

namespace {

struct TypeRegistry {
    std::mutex mutex;
    std::unordered_map<std::type_index, std::size_t> ids;
};

TypeRegistry& type_registry() {
    static TypeRegistry instance;
    return instance;
}

} // namespace

std::size_t event_type_id(std::type_index type) {
    TypeRegistry& registry = type_registry();
    const std::scoped_lock lock(registry.mutex);
    return registry.ids.try_emplace(type, registry.ids.size() + 1).first->second;
}

struct EventBus::Impl {
    struct Entry {
        std::uint64_t id;
        ErasedHandler handler;
        bool active{true};
    };

    struct QueuedEvent {
        std::size_t type;
        std::shared_ptr<const void> event;
    };

    std::unordered_map<std::size_t, std::vector<std::shared_ptr<Entry>>> handlers;
    std::uint64_t next_id{1};
    std::mutex queue_mutex;
    std::vector<QueuedEvent> queue;

    void deliver(std::size_t type, const void* event) {
        const auto found = handlers.find(type);
        if (found == handlers.end()) {
            return;
        }
        const std::vector<std::shared_ptr<Entry>> snapshot = found->second;
        for (const auto& entry : snapshot) {
            if (entry->active) {
                invoke(*entry, type, event);
            }
        }
    }

    static void invoke(const Entry& entry, std::size_t type, const void* event) noexcept {
        try {
            entry.handler(event);
        } catch (const std::exception& error) {
            log::error("event handler {} for event type {} threw: {}", entry.id, type,
                       error.what());
        } catch (...) {
            log::error("event handler {} for event type {} threw an unknown exception", entry.id,
                       type);
        }
    }
};

EventBus::EventBus() : _impl(std::make_unique<Impl>()) {}

EventBus::~EventBus() = default;

Subscription EventBus::add_handler(std::size_t type, ErasedHandler handler) {
    const std::uint64_t id = _impl->next_id++;
    _impl->handlers[type].push_back(
        std::make_shared<Impl::Entry>(Impl::Entry{.id = id, .handler = std::move(handler)}));
    return Subscription{.type = type, .id = id};
}

void EventBus::unsubscribe(Subscription subscription) {
    const auto found = _impl->handlers.find(subscription.type);
    if (found == _impl->handlers.end()) {
        return;
    }
    auto& entries = found->second;
    const auto removed = std::ranges::find_if(
        entries, [&](const auto& entry) { return entry->id == subscription.id; });
    if (removed == entries.end()) {
        return;
    }
    (*removed)->active = false;
    entries.erase(removed);
}

void EventBus::publish_erased(std::size_t type, const void* event) { _impl->deliver(type, event); }

void EventBus::enqueue_erased(std::size_t type, std::shared_ptr<const void> event) {
    const std::scoped_lock lock(_impl->queue_mutex);
    _impl->queue.push_back(Impl::QueuedEvent{.type = type, .event = std::move(event)});
}

void EventBus::dispatch() {
    std::vector<Impl::QueuedEvent> pending;
    {
        const std::scoped_lock lock(_impl->queue_mutex);
        pending.swap(_impl->queue);
    }
    for (const auto& queued : pending) {
        _impl->deliver(queued.type, queued.event.get());
    }
}

} // namespace engine::core::event
