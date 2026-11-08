#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

/** @brief What an asset file is, from its extension and, for audio, its folder. */
enum class AssetKind : std::uint8_t { sprite, sound, music, shader, other };

/** @brief A file of `<game>/assets/`. */
struct AssetEntry {
    /** Path relative to `assets/`, with `/` separators, as scripts and fields name it. */
    std::string path;
    AssetKind kind = AssetKind::other;

    bool operator==(const AssetEntry&) const = default;
};

/** @brief The most files list_assets() returns; a bigger folder is cut there. */
inline constexpr std::size_t max_assets = 2000;

/**
 * @brief Lists the regular files under `<game>/assets/`, sorted by path; hidden files and
 * folders (starting with `.`) are skipped.
 *
 * Audio files are music under a top folder named `music`, sounds elsewhere.
 * @return An empty list when the folder is missing or cannot be read.
 */
[[nodiscard]] std::vector<AssetEntry> list_assets(const std::filesystem::path& game);

/** @return "sprite", "sound", "music", "shader" or "file". */
[[nodiscard]] std::string_view kind_label(AssetKind kind);

/**
 * @return The field an asset of @p kind sets when it is dropped on an instance: `Sprite.path`
 * for a sprite, `Shader.path` for a shader, std::nullopt for the others.
 */
[[nodiscard]] std::optional<std::string_view> asset_field(AssetKind kind);

} // namespace editor
