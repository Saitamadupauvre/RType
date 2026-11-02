#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "engine/core/FieldRegistry.hpp"

namespace engine::script {

/** @brief Most arguments an event declared with `event "name" { ... }` may have. */
inline constexpr std::size_t max_event_arguments = 16;

/** @brief Longest string or asset an event argument may hold, in bytes. */
inline constexpr std::size_t max_event_text_length = 63;

/** @brief One argument of an EventCommand. */
struct EventArgument {
    /** @brief Key declared for the event. */
    std::string key;
    /** @brief Value of the declared kind: never a list. */
    core::FieldValue value;

    /** @brief Two arguments are equal when key and value are equal. */
    bool operator==(const EventArgument&) const = default;
};

/**
 * @brief One event queued by a script with `emit(name, arguments)`, for the host to publish or
 * send to clients.
 */
struct EventCommand {
    /** @brief Name declared with `event "name" { ... }`. */
    std::string name;
    /** @brief Arguments given to `emit`, in ascending key order. Keys left out are absent. */
    std::vector<EventArgument> arguments;

    /** @brief Two commands are equal when name and arguments are equal. */
    bool operator==(const EventCommand&) const = default;
};

} // namespace engine::script
