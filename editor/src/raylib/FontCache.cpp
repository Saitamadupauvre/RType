#include "raylib/FontCache.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "engine/core/log/Log.hpp"

namespace editor {

namespace {

constexpr auto files = std::to_array<std::string_view>({
    "JetBrainsMono-Regular.ttf",
    "JetBrainsMono-Bold.ttf",
});

constexpr int first_printable_ascii = 0x20;
constexpr int last_printable_ascii = 0x7E;
constexpr int first_latin_1 = 0xA0;
constexpr int last_latin_1 = 0xFF;
constexpr auto symbols = std::to_array<int>({
    0x2022,
    0x2026,
    0x2190,
    0x2192,
    0x25BE,
    0x25CF,
    0x2713,
});

std::vector<int> codepoints() {
    std::vector<int> result;
    for (int codepoint = first_printable_ascii; codepoint <= last_printable_ascii; ++codepoint) {
        result.push_back(codepoint);
    }
    for (int codepoint = first_latin_1; codepoint <= last_latin_1; ++codepoint) {
        result.push_back(codepoint);
    }
    result.insert(result.end(), symbols.begin(), symbols.end());
    return result;
}

} // namespace

FontCache::FontCache(std::filesystem::path directory) : _directory(std::move(directory)) {}

FontCache::~FontCache() {
    for (auto& [key, font] : _fonts) {
        UnloadFont(font);
    }
}

const Font& FontCache::get(Face face, std::uint16_t size) {
    const auto key = std::make_pair(face, size);
    if (const auto found = _fonts.find(key); found != _fonts.end()) {
        return found->second;
    }
    const std::string path = (_directory / files.at(static_cast<std::size_t>(face))).string();
    std::vector<int> glyphs = codepoints();
    Font font = LoadFontEx(path.c_str(), size, glyphs.data(), static_cast<int>(glyphs.size()));
    if (font.texture.id == GetFontDefault().texture.id) {
        engine::core::log::warn("editor: cannot load font {}", path);
    } else {
        engine::core::log::debug("editor: font {} at {} px", path, size);
    }
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    return _fonts.emplace(key, font).first->second;
}

} // namespace editor
