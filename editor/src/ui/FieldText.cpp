#include "ui/FieldText.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include "action/Overloaded.hpp"

namespace editor {

namespace {

constexpr std::size_t rgb_digits = 6;
constexpr std::size_t rgba_digits = 8;

std::optional<std::uint8_t> byte_at(std::string_view digits, std::size_t index) {
    const std::string_view pair = digits.substr(index * 2, 2);
    std::uint8_t byte = 0;
    const auto [end, error] = std::from_chars(pair.data(), pair.data() + pair.size(), byte, 16);
    if (error != std::errc{} || end != pair.data() + pair.size()) {
        return std::nullopt;
    }
    return byte;
}

} // namespace

std::string color_to_hex(engine::core::Rgba color) {
    return std::format("#{:02X}{:02X}{:02X}{:02X}", color.r, color.g, color.b, color.a);
}

std::optional<engine::core::Rgba> color_from_hex(std::string_view text) {
    if (text.starts_with('#')) {
        text.remove_prefix(1);
    }
    if (text.size() != rgb_digits && text.size() != rgba_digits) {
        return std::nullopt;
    }
    const auto r = byte_at(text, 0);
    const auto g = byte_at(text, 1);
    const auto b = byte_at(text, 2);
    const auto a = text.size() == rgba_digits ? byte_at(text, 3) : std::optional<std::uint8_t>{255};
    if (!r || !g || !b || !a) {
        return std::nullopt;
    }
    return engine::core::Rgba{.r = *r, .g = *g, .b = *b, .a = *a};
}

std::string describe(const engine::core::FieldValue& value) {
    return std::visit(
        Overloaded{
            [](float number) { return std::format("{:g}", number); },
            [](std::int64_t number) { return std::to_string(number); },
            [](bool flag) { return std::string(flag ? "true" : "false"); },
            [](const std::string& text) { return text; },
            [](engine::core::Vec2 vector) { return std::format("{:g}, {:g}", vector.x, vector.y); },
            [](engine::core::Rgba color) { return color_to_hex(color); },
            [](const engine::core::AssetName& asset) { return asset.path; },
            [](const engine::core::FieldList& list) {
                return std::format("{} item{}", list.items.size(), list.items.size() == 1 ? "" : "s");
            },
        },
        value);
}

std::string_view component_of(std::string_view key) {
    const std::size_t dot = key.find('.');
    return dot == std::string_view::npos ? std::string_view{} : key.substr(0, dot);
}

std::string field_label(std::string_view key) {
    const std::size_t dot = key.find('.');
    std::string label(dot == std::string_view::npos ? key : key.substr(dot + 1));
    std::ranges::replace(label, '_', ' ');
    return label;
}

} // namespace editor
