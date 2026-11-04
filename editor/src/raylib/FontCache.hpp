#pragma once

#include <raylib.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <utility>

#include "ui/Theme.hpp"

namespace editor {

class FontCache {
public:
    explicit FontCache(std::filesystem::path directory);
    ~FontCache();

    FontCache(const FontCache&) = delete;
    FontCache& operator=(const FontCache&) = delete;
    FontCache(FontCache&&) = delete;
    FontCache& operator=(FontCache&&) = delete;

    const Font& get(Face face, std::uint16_t size);

private:
    std::filesystem::path directory_;
    std::map<std::pair<Face, std::uint16_t>, Font> fonts_;
};

} // namespace editor
