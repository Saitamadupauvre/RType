#include "ValueReader.hpp"

#include <bit>
#include <cmath>
#include <string>
#include <utility>

namespace engine::network {

namespace {

std::optional<core::FieldValue> read_float(ByteReader& reader) {
    const auto bits = reader.read_u32();
    if (!bits) {
        return std::nullopt;
    }
    const auto value = std::bit_cast<float>(*bits);
    if (!std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

std::optional<core::FieldValue> read_int(ByteReader& reader) {
    const auto bits = reader.read_u32();
    if (!bits) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(static_cast<std::int32_t>(*bits));
}

std::optional<core::FieldValue> read_bool(ByteReader& reader) {
    const auto byte = reader.read_u8();
    if (!byte || *byte > 1) {
        return std::nullopt;
    }
    return *byte == 1;
}

std::optional<core::FieldValue> read_vec2(ByteReader& reader) {
    const auto x = reader.read_i16();
    const auto y = reader.read_i16();
    if (!x || !y) {
        return std::nullopt;
    }
    return core::Vec2{static_cast<float>(*x), static_cast<float>(*y)};
}

std::optional<core::FieldValue> read_color(ByteReader& reader) {
    const auto r = reader.read_u8();
    const auto g = reader.read_u8();
    const auto b = reader.read_u8();
    const auto a = reader.read_u8();
    if (!r || !g || !b || !a) {
        return std::nullopt;
    }
    return core::Rgba{.r = *r, .g = *g, .b = *b, .a = *a};
}

std::optional<core::FieldValue> read_string(ByteReader& reader) {
    auto text = read_text(reader);
    if (!text) {
        return std::nullopt;
    }
    return core::FieldValue{std::move(*text)};
}

std::optional<core::FieldValue> read_asset(ByteReader& reader) {
    auto path = read_text(reader);
    if (!path) {
        return std::nullopt;
    }
    return core::AssetName{std::move(*path)};
}

} // namespace

std::optional<std::string> read_text(ByteReader& reader) {
    const auto length = reader.read_u8();
    const auto bytes = length ? reader.read_bytes(*length) : std::nullopt;
    if (!bytes) {
        return std::nullopt;
    }
    return std::string(bytes->begin(), bytes->end());
}

std::optional<core::FieldValue> read_value(ByteReader& reader, std::uint8_t kind) {
    switch (static_cast<core::FieldKind>(kind)) {
    case core::FieldKind::Float:
        return read_float(reader);
    case core::FieldKind::Int:
        return read_int(reader);
    case core::FieldKind::Bool:
        return read_bool(reader);
    case core::FieldKind::String:
        return read_string(reader);
    case core::FieldKind::Vec2:
        return read_vec2(reader);
    case core::FieldKind::Color:
        return read_color(reader);
    case core::FieldKind::Asset:
        return read_asset(reader);
    default:
        return std::nullopt;
    }
}

} // namespace engine::network
