#include "engine/core/AssetCache.hpp"

#include <algorithm>
#include <cctype>
#include <ranges>
#include <string_view>

namespace engine::core {

namespace {

bool is_name_character(char character) noexcept {
    return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_' ||
           character == '-' || character == '.';
}

bool is_valid_segment(std::string_view segment) noexcept {
    return !segment.empty() && segment.front() != '.' &&
           std::ranges::all_of(segment, is_name_character);
}

} // namespace

bool is_valid_asset_name(std::string_view name) noexcept {
    if (name.empty() || name.size() > max_asset_name_length) {
        return false;
    }
    return std::ranges::all_of(std::views::split(name, '/'), [](const auto& segment) {
        return is_valid_segment(std::string_view(segment.begin(), segment.end()));
    });
}

} // namespace engine::core
