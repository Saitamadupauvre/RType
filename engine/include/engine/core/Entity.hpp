#pragma once

#include <cstdint>

namespace engine::core {

/**
 * @brief Handle to an entity: an index and a generation packed in one integer.
 *
 * The index (low 32 bits) identifies a slot in the EntityPool. The generation
 * (high 32 bits) changes every time the slot is recycled, so a handle kept after
 * its entity was destroyed never matches the new entity using the same slot.
 *
 * @note Handles are plain values: cheap to copy and compare.
 */
class Entity {
public:
    using Id = std::uint64_t;

    /**
     * @brief Builds a handle from its index and generation.
     * @param index Slot of the entity in its pool.
     * @param generation Number of times the slot was recycled.
     */
    constexpr Entity(std::uint32_t index, std::uint32_t generation) noexcept
        : _id((static_cast<Id>(generation) << generation_shift) | index) {}

    /**
     * @brief Rebuilds a handle from its packed representation.
     * @param id Value previously returned by id().
     */
    constexpr explicit Entity(Id id) noexcept : _id(id) {}

    /** @return The slot of the entity in its pool. */
    [[nodiscard]] constexpr std::uint32_t index() const noexcept {
        return static_cast<std::uint32_t>(_id);
    }

    /** @return The generation of the slot when the handle was created. */
    [[nodiscard]] constexpr std::uint32_t generation() const noexcept {
        return static_cast<std::uint32_t>(_id >> generation_shift);
    }

    /** @return The packed index and generation. */
    [[nodiscard]] constexpr Id id() const noexcept { return _id; }

    constexpr bool operator==(const Entity&) const noexcept = default;

private:
    static constexpr unsigned generation_shift = 32;

    Id _id;
};

} // namespace engine::core
