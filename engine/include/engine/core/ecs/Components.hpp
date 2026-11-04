#pragma once

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace engine::core {

/** @brief 8-bit RGBA color. Defaults to opaque white. */
struct Rgba {
    std::uint8_t r{255};
    std::uint8_t g{255};
    std::uint8_t b{255};
    std::uint8_t a{255};

    bool operator==(const Rgba&) const = default;
};

/**
 * @brief Image drawn at an entity's position, tinted by a color.
 *
 * @note The path is relative to the assets folder. An empty path means no image: renderers may
 * then draw a plain shape filled with the color.
 */
struct Sprite {
    std::string path;
    Rgba color;
};

/**
 * @brief Drawing order of an entity among the others.
 *
 * @note Entities with a lower z_index are drawn first, so higher ones appear in front. Entities
 * with the same z_index, or without this component (read as 0), are drawn in spawn order.
 */
struct Layer {
    std::int32_t z_index{0};
};

/**
 * @brief Shader program an entity is drawn through.
 *
 * @note The path is relative to the assets folder and has no extension, such as `shaders/space`.
 * An empty path draws the entity with the default pipeline.
 */
struct Shader {
    std::string path;
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
