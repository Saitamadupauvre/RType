#include "JsonFieldValue.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "engine/core/ecs/Components.hpp"

namespace engine::script {

namespace {

constexpr std::size_t vec2_size = 2;
constexpr std::size_t color_size = 4;
constexpr std::int64_t max_channel = 255;

[[noreturn]] void reject(const Json& value, std::string_view expected) {
    throw std::invalid_argument(std::format("expected {}, got {}", expected, value.type_name()));
}

float float_from_json(const Json& value) {
    if (!value.is_number()) {
        reject(value, "a number");
    }
    const auto number = value.get<double>();
    if (std::abs(number) > static_cast<double>(std::numeric_limits<float>::max())) {
        throw std::invalid_argument(std::format("{} does not fit in a float", number));
    }
    return static_cast<float>(number);
}

std::int64_t int_from_json(const Json& value) {
    if (!value.is_number_integer()) {
        reject(value, "an integer");
    }
    if (value.is_number_unsigned() &&
        value.get<std::uint64_t>() >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        throw std::invalid_argument(std::format("{} does not fit in an int", value.dump()));
    }
    return value.get<std::int64_t>();
}

bool bool_from_json(const Json& value) {
    if (!value.is_boolean()) {
        reject(value, "a boolean");
    }
    return value.get<bool>();
}

std::string string_from_json(const Json& value) {
    if (!value.is_string()) {
        reject(value, "a string");
    }
    return value.get<std::string>();
}

std::uint8_t channel_from_json(const Json& value) {
    const std::int64_t channel = int_from_json(value);
    if (channel < 0 || channel > max_channel) {
        throw std::invalid_argument(std::format("color channel {} is not in 0..255", channel));
    }
    return static_cast<std::uint8_t>(channel);
}

core::Rgba color_from_json(const Json& value) {
    if (!value.is_array() || value.size() != color_size) {
        reject(value, "a color [r, g, b, a]");
    }
    return {.r = channel_from_json(value[0]),
            .g = channel_from_json(value[1]),
            .b = channel_from_json(value[2]),
            .a = channel_from_json(value[3])};
}

core::ScalarValue scalar_from_json(const Json& value, core::FieldKind kind) {
    switch (kind) {
    case core::FieldKind::Float:
        return float_from_json(value);
    case core::FieldKind::Int:
        return int_from_json(value);
    case core::FieldKind::Bool:
        return bool_from_json(value);
    case core::FieldKind::String:
        return string_from_json(value);
    case core::FieldKind::Vec2:
        return vec2_from_json(value);
    case core::FieldKind::Color:
        return color_from_json(value);
    case core::FieldKind::Asset:
        return core::AssetName{string_from_json(value)};
    case core::FieldKind::List:
        break;
    }
    throw std::invalid_argument(
        std::format("a list item cannot be a {}", core::field_kind_name(kind)));
}

core::FieldList list_from_json(const Json& value, core::FieldKind element) {
    if (!value.is_array()) {
        reject(value, "an array");
    }
    core::FieldList list{.element = element, .items = {}};
    list.items.reserve(value.size());
    for (const Json& item : value) {
        list.items.push_back(scalar_from_json(item, element));
    }
    return list;
}

double shortest(float value) {
    std::array<char, std::numeric_limits<float>::max_digits10 + 16> text{};
    const auto written = std::to_chars(text.data(), text.data() + text.size() - 1, value);
    *written.ptr = '\0';
    return std::strtod(text.data(), nullptr);
}

Json scalar_to_json(const core::ScalarValue& value) {
    return std::visit(
        [](const auto& item) -> Json {
            using Item = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Item, float>) {
                return shortest(item);
            } else if constexpr (std::is_same_v<Item, core::Vec2>) {
                return vec2_to_json(item);
            } else if constexpr (std::is_same_v<Item, core::Rgba>) {
                return Json::array({item.r, item.g, item.b, item.a});
            } else if constexpr (std::is_same_v<Item, core::AssetName>) {
                return item.path;
            } else {
                return item;
            }
        },
        value);
}

Json list_to_json(const core::FieldList& list) {
    Json items = Json::array();
    for (const core::ScalarValue& item : list.items) {
        items.push_back(scalar_to_json(item));
    }
    return items;
}

core::FieldValue to_field_value(core::ScalarValue scalar) {
    return std::visit(
        [](auto&& item) -> core::FieldValue { return std::forward<decltype(item)>(item); },
        std::move(scalar));
}

} // namespace

core::FieldValue field_from_json(const Json& value, core::FieldKind kind,
                                 std::optional<core::FieldKind> element) {
    if (kind == core::FieldKind::List) {
        return list_from_json(value, element.value_or(core::FieldKind::String));
    }
    return to_field_value(scalar_from_json(value, kind));
}

core::Vec2 vec2_from_json(const Json& value) {
    if (!value.is_array() || value.size() != vec2_size) {
        reject(value, "a vec2 [x, y]");
    }
    return {float_from_json(value[0]), float_from_json(value[1])};
}

Json field_to_json(const core::FieldValue& value) {
    return std::visit(
        [](const auto& item) -> Json {
            using Item = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Item, core::FieldList>) {
                return list_to_json(item);
            } else {
                return scalar_to_json(core::ScalarValue{item});
            }
        },
        value);
}

Json vec2_to_json(core::Vec2 value) { return Json::array({shortest(value.x), shortest(value.y)}); }

} // namespace engine::script
