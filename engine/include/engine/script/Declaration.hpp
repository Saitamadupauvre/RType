#pragma once

#include <cstdint>
#include <filesystem>

namespace engine::script {

/**
 * @brief What a script file declares with `entity "name"` or `system "name"`.
 */
enum class DeclarationKind : std::uint8_t { Entity, System };

/**
 * @brief Read-only description of a registered declaration.
 */
struct DeclarationInfo {
    /** @brief Whether the name was declared with `entity` or `system`. */
    DeclarationKind kind;
    /** @brief File that declared the name, relative to the script folder. */
    std::filesystem::path source;

    /** @brief Two descriptions are equal when kind and source path are equal. */
    bool operator==(const DeclarationInfo&) const = default;
};

} // namespace engine::script
