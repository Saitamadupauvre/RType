#include "assets/AssetList.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <system_error>

namespace editor {

namespace {

namespace fs = std::filesystem;

constexpr std::array sprite_extensions{".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif", ".qoi"};
constexpr std::array audio_extensions{".wav", ".ogg", ".mp3", ".flac", ".qoa"};
constexpr std::array shader_extensions{".fs", ".vs", ".glsl", ".frag", ".vert"};

template <std::size_t Size>
bool one_of(const std::array<const char*, Size>& extensions, std::string_view extension) {
    return std::ranges::any_of(extensions,
                               [extension](std::string_view known) { return known == extension; });
}

std::string lowercase(std::string text) {
    std::ranges::transform(text, text.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

AssetKind kind_of(const fs::path& relative) {
    const std::string extension = lowercase(relative.extension().string());
    if (one_of(sprite_extensions, extension)) {
        return AssetKind::sprite;
    }
    if (one_of(shader_extensions, extension)) {
        return AssetKind::shader;
    }
    if (one_of(audio_extensions, extension)) {
        return *relative.begin() == "music" ? AssetKind::music : AssetKind::sound;
    }
    return AssetKind::other;
}

bool hidden(const fs::path& path) { return path.filename().string().starts_with('.'); }

} // namespace

std::vector<AssetEntry> list_assets(const fs::path& game) {
    const fs::path root = game / "assets";
    std::vector<AssetEntry> assets;
    std::error_code error;
    fs::recursive_directory_iterator walk(root, fs::directory_options::none, error);
    for (; !error && walk != fs::recursive_directory_iterator(); walk.increment(error)) {
        const fs::directory_entry& entry = *walk;
        if (hidden(entry.path())) {
            if (entry.is_directory(error)) {
                walk.disable_recursion_pending();
            }
            continue;
        }
        if (!entry.is_regular_file(error)) {
            continue;
        }
        const fs::path relative = entry.path().lexically_relative(root);
        assets.push_back({.path = relative.generic_string(), .kind = kind_of(relative)});
        if (assets.size() == max_assets) {
            break;
        }
    }
    std::ranges::sort(assets, {}, &AssetEntry::path);
    return assets;
}

std::string_view kind_label(AssetKind kind) {
    switch (kind) {
    case AssetKind::sprite:
        return "sprite";
    case AssetKind::sound:
        return "sound";
    case AssetKind::music:
        return "music";
    case AssetKind::shader:
        return "shader";
    case AssetKind::other:
        break;
    }
    return "file";
}

std::optional<std::string_view> asset_field(AssetKind kind) {
    switch (kind) {
    case AssetKind::sprite:
        return "Sprite.path";
    case AssetKind::shader:
        return "Shader.path";
    case AssetKind::sound:
    case AssetKind::music:
    case AssetKind::other:
        break;
    }
    return std::nullopt;
}

} // namespace editor
