#include "FieldWire.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <variant>

#include "Protocol.hpp"

namespace engine::network {

namespace {

constexpr std::size_t number_size = 4;
constexpr std::size_t bool_size = 1;
constexpr std::size_t text_length_size = 1;
constexpr char component_separator = '.';

std::size_t text_size(const std::string& text) noexcept { return text_length_size + text.size(); }

struct ValueSize {
    std::size_t operator()(float) const noexcept { return number_size; }
    std::size_t operator()(std::int64_t) const noexcept { return number_size; }
    std::size_t operator()(bool) const noexcept { return bool_size; }
    std::size_t operator()(const std::string& text) const noexcept { return text_size(text); }
    std::size_t operator()(const core::Vec2&) const noexcept { return number_size; }
    std::size_t operator()(const core::Rgba&) const noexcept { return number_size; }
    std::size_t operator()(const core::AssetName& asset) const noexcept {
        return text_size(asset.path);
    }
    std::size_t operator()(const core::FieldList&) const noexcept { return 0; }
};

const std::string* text_of(const core::FieldValue& value) noexcept {
    if (const auto* text = std::get_if<std::string>(&value)) {
        return text;
    }
    if (const auto* asset = std::get_if<core::AssetName>(&value)) {
        return &asset->path;
    }
    return nullptr;
}

} // namespace

std::int16_t to_wire_coordinate(float value) noexcept {
    if (std::isnan(value)) {
        return 0;
    }
    constexpr auto lowest = static_cast<float>(std::numeric_limits<std::int16_t>::min());
    constexpr auto highest = static_cast<float>(std::numeric_limits<std::int16_t>::max());
    return static_cast<std::int16_t>(std::clamp(std::round(value), lowest, highest));
}

std::int32_t to_wire_int(std::int64_t value) noexcept {
    constexpr std::int64_t lowest = std::numeric_limits<std::int32_t>::min();
    constexpr std::int64_t highest = std::numeric_limits<std::int32_t>::max();
    return static_cast<std::int32_t>(std::clamp(value, lowest, highest));
}

std::string wire_field_name(core::FieldRef field) {
    return field.component->name() + component_separator + field.field->name();
}

core::FieldRef find_wire_field(const core::FieldRegistry& registry, std::string_view name) {
    const auto separator = name.find(component_separator);
    if (separator == std::string_view::npos) {
        return {};
    }
    return registry.find_field(name.substr(0, separator), name.substr(separator + 1));
}

bool is_replicable(core::FieldKind kind) noexcept {
    return kind != core::FieldKind::List &&
           static_cast<std::size_t>(kind) < core::field_kinds.size();
}

std::size_t max_wire_size(core::FieldKind kind) noexcept {
    switch (kind) {
    case core::FieldKind::Bool:
        return bool_size;
    case core::FieldKind::String:
    case core::FieldKind::Asset:
        return text_length_size + max_wire_text_length;
    default:
        return number_size;
    }
}

bool fits_wire(const core::FieldValue& value) noexcept {
    if (!is_replicable(core::kind_of(value))) {
        return false;
    }
    if (const auto* number = std::get_if<float>(&value)) {
        return std::isfinite(*number);
    }
    const std::string* text = text_of(value);
    return text == nullptr || text->size() <= max_wire_text_length;
}

std::size_t wire_size(const core::FieldValue& value) noexcept {
    return std::visit(ValueSize{}, value);
}

std::size_t wire_size(const EntityState& entity) noexcept {
    std::size_t size = entity_header_size;
    for (const FieldState& field : entity.fields) {
        size += field_header_size + wire_size(field.value);
    }
    return size;
}

} // namespace engine::network
