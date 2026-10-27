#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "engine/core/ComponentTypeId.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/Entity.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/Export.hpp"
#include "engine/core/Vec2.hpp"

namespace engine::core {

/** @brief Name of an asset (texture, sound, shader...), relative to the assets folder. */
struct AssetName {
    std::string path;

    bool operator==(const AssetName&) const = default;
};

/** @brief Value of one component field, whatever the concrete component type. */
using FieldValue = std::variant<float, std::int64_t, bool, std::string, Vec2, Rgba, AssetName>;

/** @brief Closed list of field kinds, in the order of the FieldValue alternatives. */
enum class FieldKind : std::uint8_t { Float, Int, Bool, String, Vec2, Color, Asset };

/** @brief A field kind and its lowercase name, used in messages and files. */
struct FieldKindInfo {
    FieldKind kind;
    std::string_view name;
};

/** @brief Every field kind, indexed by its FieldKind value. */
inline constexpr std::array field_kinds{
    FieldKindInfo{.kind = FieldKind::Float, .name = "float"},
    FieldKindInfo{.kind = FieldKind::Int, .name = "int"},
    FieldKindInfo{.kind = FieldKind::Bool, .name = "bool"},
    FieldKindInfo{.kind = FieldKind::String, .name = "string"},
    FieldKindInfo{.kind = FieldKind::Vec2, .name = "vec2"},
    FieldKindInfo{.kind = FieldKind::Color, .name = "color"},
    FieldKindInfo{.kind = FieldKind::Asset, .name = "asset"},
};

namespace detail {

template <typename T, typename Variant> struct AlternativeIndex;

template <typename T, typename... Alternatives>
struct AlternativeIndex<T, std::variant<Alternatives...>> {
    static constexpr std::array matches{std::same_as<T, Alternatives>...};
    static constexpr auto value =
        static_cast<std::size_t>(std::ranges::find(matches, true) - matches.begin());
};

} // namespace detail

/** @brief Types that a FieldValue can hold. */
template <typename T>
concept FieldType =
    detail::AlternativeIndex<T, FieldValue>::value < std::variant_size_v<FieldValue>;

/** @return The kind matching the C++ type @p T. */
template <FieldType T> [[nodiscard]] constexpr FieldKind field_kind_of() noexcept {
    return static_cast<FieldKind>(detail::AlternativeIndex<T, FieldValue>::value);
}

/** @return The kind held by @p value. */
[[nodiscard]] constexpr FieldKind kind_of(const FieldValue& value) noexcept {
    return static_cast<FieldKind>(value.index());
}

/** @return A lowercase name such as "float" or "vec2", or "unknown" for an invalid kind. */
[[nodiscard]] constexpr std::string_view field_kind_name(FieldKind kind) noexcept {
    const auto index = static_cast<std::size_t>(kind);
    return index < field_kinds.size() ? field_kinds[index].name : "unknown";
}

/** @brief What consumers may do with a field. */
struct FieldFlags {
    /** Sent to clients in snapshots. */
    bool replicated = false;
    /** Shown and changed in the editor inspector. */
    bool editable = false;

    bool operator==(const FieldFlags&) const = default;
};

/** @brief Common FieldFlags combinations. */
namespace field_flags {

/** @brief Shown in the inspector, kept on the machine that owns the entity. */
inline constexpr FieldFlags editable{.replicated = false, .editable = true};

/** @brief Shown in the inspector and sent to clients in snapshots. */
inline constexpr FieldFlags replicated{.replicated = true, .editable = true};

} // namespace field_flags

/**
 * @brief One named, typed field of a component.
 *
 * Reads and writes go through FieldRegistry, which checks kinds and presence first.
 */
class FieldDescriptor {
public:
    using Getter = std::function<FieldValue(EntityManager&, Entity)>;
    using Setter = std::function<void(EntityManager&, Entity, const FieldValue&)>;

    /**
     * @param getter Reads the field of an entity owning the component.
     * @param setter Writes a value of @p kind, adding the component when the entity has none.
     * Throws std::out_of_range for a value the component cannot hold.
     * @note Prefer member_field, asset_field or component_field.
     */
    FieldDescriptor(std::string name, FieldKind kind, FieldFlags flags, Getter getter,
                    Setter setter)
        : _name(std::move(name)), _kind(kind), _flags(flags), _getter(std::move(getter)),
          _setter(std::move(setter)) {}

    [[nodiscard]] const std::string& name() const noexcept { return _name; }
    [[nodiscard]] FieldKind kind() const noexcept { return _kind; }
    [[nodiscard]] FieldFlags flags() const noexcept { return _flags; }

private:
    friend class FieldRegistry;

    std::string _name;
    FieldKind _kind;
    FieldFlags _flags;
    Getter _getter;
    Setter _setter;
};

/** @brief A component known by name, with the list of its fields. Built by FieldRegistry. */
class ComponentDescriptor {
public:
    [[nodiscard]] const std::string& name() const noexcept { return _name; }
    [[nodiscard]] std::span<const FieldDescriptor> fields() const noexcept { return _fields; }

    /** @return The field called @p field_name, or nullptr. */
    [[nodiscard]] ENGINE_CORE_EXPORT const FieldDescriptor*
    find_field(std::string_view field_name) const noexcept;

private:
    friend class FieldRegistry;

    using Presence = bool (*)(const EntityManager&, Entity);

    ComponentDescriptor(std::string name, ComponentTypeId type, Presence present,
                        std::vector<FieldDescriptor> fields)
        : _name(std::move(name)), _type(type), _present(present), _fields(std::move(fields)) {}

    std::string _name;
    ComponentTypeId _type;
    Presence _present;
    std::vector<FieldDescriptor> _fields;
};

/** @brief A field together with the component it belongs to, as found in a FieldRegistry. */
struct FieldRef {
    const ComponentDescriptor* component = nullptr;
    const FieldDescriptor* field = nullptr;

    /** @return false for an empty FieldRef, such as a failed lookup. */
    [[nodiscard]] explicit operator bool() const noexcept { return field != nullptr; }
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4275)
#endif

/** @brief Thrown when a component name or type is registered twice. */
class ENGINE_CORE_EXPORT DuplicateComponentName : public std::invalid_argument {
public:
    explicit DuplicateComponentName(std::string_view component);
};

/** @brief Thrown when a component lists two fields with the same name. */
class ENGINE_CORE_EXPORT DuplicateFieldName : public std::invalid_argument {
public:
    DuplicateFieldName(std::string_view component, std::string_view field);
};

/** @brief Thrown when a component or field name is not registered. */
class ENGINE_CORE_EXPORT UnknownField : public std::out_of_range {
public:
    UnknownField(std::string_view component, std::string_view field);
};

/** @brief Thrown when a field is given a value of another kind. */
class ENGINE_CORE_EXPORT FieldKindMismatch : public std::invalid_argument {
public:
    FieldKindMismatch(std::string_view component, std::string_view field, FieldKind expected,
                      FieldKind actual);
};

/** @brief Thrown when a field is given a value its component cannot hold. */
class ENGINE_CORE_EXPORT FieldValueOutOfRange : public std::out_of_range {
public:
    FieldValueOutOfRange(std::string_view component, std::string_view field,
                         std::string_view reason);
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

/**
 * @brief Builds a field from accessors working on the concrete component.
 *
 * @tparam Component Component type the field belongs to.
 * @param read Callable `FieldValue(const Component&)`.
 * @param write Callable `void(Component&, const FieldValue&)`, given a value of @p kind. It
 * rejects a value by throwing std::out_of_range.
 * @note A throwing @p write leaves the entity unchanged.
 */
template <typename Component, typename Read, typename Write>
[[nodiscard]] FieldDescriptor component_field(std::string name, FieldKind kind, FieldFlags flags,
                                              Read read, Write write) {
    return FieldDescriptor{std::move(name), kind, flags,
                           [read](EntityManager& world, Entity entity) -> FieldValue {
                               return read(std::as_const(world.get<Component>(entity)));
                           },
                           [write](EntityManager& world, Entity entity, const FieldValue& value) {
                               Component component = world.has<Component>(entity)
                                                         ? world.get<Component>(entity)
                                                         : Component{};
                               write(component, value);
                               world.add(entity, std::move(component));
                           }};
}

namespace detail {

template <typename Member> struct MemberPointer;

template <typename Owner, typename Value> struct MemberPointer<Value Owner::*> {
    using Component = Owner;
    using Type = Value;
};

template <auto Member> using MemberComponent = MemberPointer<decltype(Member)>::Component;
template <auto Member> using MemberValue = MemberPointer<decltype(Member)>::Type;

template <typename T>
concept NarrowInteger =
    std::integral<T> && !std::same_as<T, bool> && !std::same_as<T, std::int64_t>;

} // namespace detail

/**
 * @brief Builds a field reading and writing one data member.
 *
 * @tparam Member Pointer to a data member whose type is a FieldType or an integer. Integers are
 * exposed as FieldKind::Int; writing a value they cannot hold throws std::out_of_range.
 */
template <auto Member>
    requires FieldType<detail::MemberValue<Member>> ||
             detail::NarrowInteger<detail::MemberValue<Member>>
[[nodiscard]] FieldDescriptor member_field(std::string name, FieldFlags flags = {}) {
    using Component = detail::MemberComponent<Member>;
    using Value = detail::MemberValue<Member>;
    if constexpr (detail::NarrowInteger<Value>) {
        return component_field<Component>(
            std::move(name), FieldKind::Int, flags,
            [](const Component& component) {
                return FieldValue{static_cast<std::int64_t>(component.*Member)};
            },
            [](Component& component, const FieldValue& value) {
                const auto number = std::get<std::int64_t>(value);
                if (!std::in_range<Value>(number)) {
                    throw std::out_of_range("integer does not fit the member");
                }
                component.*Member = static_cast<Value>(number);
            });
    } else {
        return component_field<Component>(
            std::move(name), field_kind_of<Value>(), flags,
            [](const Component& component) {
                return FieldValue{std::in_place_type<Value>, component.*Member};
            },
            [](Component& component, const FieldValue& value) {
                component.*Member = std::get<Value>(value);
            });
    }
}

/**
 * @brief Builds an asset field over a std::string data member holding an asset path.
 *
 * @tparam Member Pointer to a std::string data member.
 */
template <auto Member>
    requires std::same_as<detail::MemberValue<Member>, std::string>
[[nodiscard]] FieldDescriptor asset_field(std::string name, FieldFlags flags = {}) {
    using Component = detail::MemberComponent<Member>;
    return component_field<Component>(
        std::move(name), FieldKind::Asset, flags,
        [](const Component& component) { return FieldValue{AssetName{component.*Member}}; },
        [](Component& component, const FieldValue& value) {
            component.*Member = std::get<AssetName>(value).path;
        });
}

/**
 * @brief Describes the fields of components so that tools can read and write them by name.
 *
 * Lua bindings, the editor inspector, scene files and snapshots all go through the same registry
 * instead of knowing each component type. Look a field up once with find_field() or fields_of(),
 * then read and write it through its FieldRef.
 *
 * @note Pointers, spans and FieldRefs stay valid until the next add().
 * @note Not thread safe: fill it at startup, then share it read-only.
 */
class FieldRegistry {
public:
    /**
     * @brief Registers a component type under @p name.
     *
     * @tparam Component Type every field of @p fields was built for.
     * @throws DuplicateComponentName If @p name or @p Component is already registered.
     * @throws DuplicateFieldName If two fields share a name.
     * @throws std::invalid_argument If a name is empty or a field has no getter or setter.
     */
    template <typename Component> void add(std::string name, std::vector<FieldDescriptor> fields) {
        add(ComponentDescriptor{std::move(name), component_type_id<Component>(), &owns<Component>,
                                std::move(fields)});
    }

    /** @return The component called @p name, or nullptr. */
    [[nodiscard]] ENGINE_CORE_EXPORT const ComponentDescriptor*
    find_component(std::string_view name) const noexcept;

    /** @return The field @p field of component @p component, or an empty FieldRef. */
    [[nodiscard]] ENGINE_CORE_EXPORT FieldRef find_field(std::string_view component,
                                                         std::string_view field) const noexcept;

    /** @return Every component, in registration order. */
    [[nodiscard]] ENGINE_CORE_EXPORT std::span<const ComponentDescriptor>
    components() const noexcept;

    /** @return Every field of every component owned by @p entity, in registration order. */
    [[nodiscard]] ENGINE_CORE_EXPORT std::vector<FieldRef> fields_of(const EntityManager& world,
                                                                     Entity entity) const;

    /**
     * @brief Reads a field of an entity.
     *
     * @param field A FieldRef returned by this registry.
     * @return The value, or std::nullopt if the entity does not own the component.
     * @throws std::invalid_argument If @p field is empty.
     */
    [[nodiscard]] ENGINE_CORE_EXPORT std::optional<FieldValue>
    get(EntityManager& world, Entity entity, FieldRef field) const;

    /**
     * @brief Reads a field of an entity by name.
     *
     * @throws UnknownField If the component or the field is not registered.
     */
    [[nodiscard]] ENGINE_CORE_EXPORT std::optional<FieldValue> get(EntityManager& world,
                                                                   Entity entity,
                                                                   std::string_view component,
                                                                   std::string_view field) const;

    /**
     * @brief Writes a field of an entity, adding the component with default values if needed.
     *
     * @param field A FieldRef returned by this registry.
     * @throws std::invalid_argument If @p field is empty or the entity is not alive.
     * @throws FieldKindMismatch If @p value is not of the field's kind; the entity is unchanged.
     * @throws FieldValueOutOfRange If the component cannot hold @p value; the entity is unchanged.
     * @throws ComponentNotRegistered If the world does not know the component type.
     */
    ENGINE_CORE_EXPORT void set(EntityManager& world, Entity entity, FieldRef field,
                                const FieldValue& value) const;

    /**
     * @brief Writes a field of an entity by name.
     *
     * @throws UnknownField If the component or the field is not registered.
     * @note Throws everything the FieldRef overload throws.
     */
    ENGINE_CORE_EXPORT void set(EntityManager& world, Entity entity, std::string_view component,
                                std::string_view field, const FieldValue& value) const;

private:
    template <typename Component> static bool owns(const EntityManager& world, Entity entity) {
        return world.is_registered<Component>() && world.has<Component>(entity);
    }

    ENGINE_CORE_EXPORT void add(ComponentDescriptor component);
    [[nodiscard]] FieldRef require(std::string_view component, std::string_view field) const;

    std::vector<ComponentDescriptor> _components;
};

/**
 * @brief Registers the fields of the engine-core components: Position, Velocity and Sprite.
 *
 * @throws DuplicateComponentName If one of them is already registered.
 */
ENGINE_CORE_EXPORT void register_core_fields(FieldRegistry& registry);

} // namespace engine::core
