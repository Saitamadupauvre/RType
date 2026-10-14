#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "engine/core/Vec2.hpp"

namespace engine::core {

/**
 * @brief Image drawn at an entity's position.
 *
 * @note The path is relative to the assets folder. An empty path means no image.
 */
struct Sprite {
    std::string path;
};

/**
 * @brief Axis-aligned box used for overlap tests.
 *
 * @note The box starts at the entity's position plus offset and spans size. A size with a zero
 * or negative side never overlaps anything.
 */
struct Hitbox {
    Vec2 offset;
    Vec2 size;
};

/**
 * @brief Set of labels attached to an entity, such as "enemy" or "player".
 *
 * @note Labels are unique: adding one twice keeps a single copy. Order of insertion is kept.
 */
class Tags {
public:
    Tags() = default;

    /** @param labels Initial labels; duplicates are dropped. */
    Tags(std::initializer_list<std::string> labels) {
        for (const std::string& label : labels) {
            add(label);
        }
    }

    /** @return true if @p label is present. */
    [[nodiscard]] bool has(std::string_view label) const {
        return std::ranges::find(_labels, label) != _labels.end();
    }

    /**
     * @brief Adds a label.
     *
     * @return true if it was added, false if it was already present.
     */
    bool add(std::string label) {
        if (has(label)) {
            return false;
        }
        _labels.push_back(std::move(label));
        return true;
    }

    /**
     * @brief Removes a label.
     *
     * @return true if it was present.
     */
    bool remove(std::string_view label) {
        const auto found = std::ranges::find(_labels, label);
        if (found == _labels.end()) {
            return false;
        }
        _labels.erase(found);
        return true;
    }

    /** @return Every label, in insertion order. */
    [[nodiscard]] const std::vector<std::string>& labels() const noexcept { return _labels; }

private:
    std::vector<std::string> _labels;
};

} // namespace engine::core
