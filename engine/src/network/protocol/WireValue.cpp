#include "WireValue.hpp"

#include <cstddef>
#include <string>
#include <type_traits>
#include <variant>

namespace engine::network {

namespace {

static_assert(std::variant_size_v<WireValue> == static_cast<std::size_t>(core::FieldKind::List));
static_assert(std::is_same_v<std::variant_alternative_t<6, WireValue>, AssetId>);
static_assert(std::is_same_v<std::variant_alternative_t<6, core::FieldValue>, core::AssetName>);

struct ToWire {
    const StringTable& strings;
    StringCategory category;

    std::optional<WireValue> operator()(const core::AssetName& asset) const {
        if (asset.path.empty()) {
            return AssetId{};
        }
        const auto id = strings.id_of(category, asset.path);
        if (!id) {
            return std::nullopt;
        }
        return AssetId{*id};
    }

    std::optional<WireValue> operator()(const core::FieldList&) const { return std::nullopt; }

    template <typename Value> std::optional<WireValue> operator()(const Value& value) const {
        return WireValue{value};
    }
};

struct FromWire {
    const StringTable& strings;
    StringCategory category;

    std::optional<core::FieldValue> operator()(AssetId asset) const {
        if (asset.value == no_asset) {
            return core::AssetName{};
        }
        const std::string* name = strings.name_of(category, asset.value);
        if (name == nullptr) {
            return std::nullopt;
        }
        return core::AssetName{*name};
    }

    template <typename Value> std::optional<core::FieldValue> operator()(const Value& value) const {
        return core::FieldValue{value};
    }
};

} // namespace

core::FieldKind kind_of(const WireValue& value) noexcept {
    return static_cast<core::FieldKind>(value.index());
}

StringCategory asset_category(core::AssetType type) noexcept {
    return type == core::AssetType::Shader ? StringCategory::shader : StringCategory::texture;
}

std::optional<WireValue> to_wire(const core::FieldValue& value, const StringTable& strings,
                                 core::AssetType asset) {
    return std::visit(ToWire{.strings = strings, .category = asset_category(asset)}, value);
}

std::optional<core::FieldValue> from_wire(const WireValue& value, const StringTable& strings,
                                          core::AssetType asset) {
    return std::visit(FromWire{.strings = strings, .category = asset_category(asset)}, value);
}

} // namespace engine::network
