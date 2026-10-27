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
#include <typeinfo>
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

/** @brief Value of one list item: any field kind except List. */
using ScalarValue = std::variant<float, std::int64_t, bool, std::string, Vec2, Rgba, AssetName>;

/** @brief Closed list of field kinds, in the order of the FieldValue alternatives. */
enum class FieldKind : std::uint8_t { Float, Int, Bool, String, Vec2, Color, Asset, List };

/**
 * @brief Ordered values of one scalar kind, such as tags or waypoints.
 *
 * @note Lists never nest: @ref element is never FieldKind::List.
 */
struct FieldList {
    /** Kind of every item. */
    FieldKind element = FieldKind::String;
    std::vector<ScalarValue> items;

    bool operator==(const FieldList&) const = default;
};

/** @brief Value of one component field, whatever the concrete component type. */
using FieldValue =
    std::variant<float, std::int64_t, bool, std::string, Vec2, Rgba, AssetName, FieldList>;

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
    FieldKindInfo{.kind = FieldKind::List, .name = "list"},
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

/** @return The kind held by @p value, never FieldKind::List. */
[[nodiscard]] constexpr FieldKind kind_of(const ScalarValue& value) noexcept {
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
     * @param element Item kind, required when @p kind is FieldKind::List and empty otherwise.
     * @note Prefer member_field, asset_field, component_field or list_field.
     */
    FieldDescriptor(std::string name, FieldKind kind, FieldFlags flags, Getter getter,
                    Setter setter, std::optional<FieldKind> element = std::nullopt)
        : _name(std::move(name)), _kind(kind), _element(element), _flags(flags),
          _getter(std::move(getter)), _setter(std::move(setter)) {}

    [[nodiscard]] const std::string& name() const noexcept { return _name; }
    [[nodiscard]] FieldKind kind() const noexcept { return _kind; }
    /** @return The item kind of a list field, std::nullopt for any other kind. */
    [[nodiscard]] std::optional<FieldKind> element_kind() const noexcept { return _element; }
    [[nodiscard]] FieldFlags flags() const noexcept { return _flags; }

private:
    friend class FieldRegistry;

    std::string _name;
    FieldKind _kind;
    std::optional<FieldKind> _element;
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

    struct Requirement {
        bool (*registered)(const EntityManager&);
        void (*ensure)(EntityManager&, Entity);
        const char* type_name;
    };

    ComponentDescriptor(std::string name, ComponentTypeId type, Presence present,
                        std::vector<FieldDescriptor> fields, std::vector<Requirement> required)
        : _name(std::move(name)), _type(type), _present(present), _fields(std::move(fields)),
          _required(std::move(required)) {}

    std::string _name;
    ComponentTypeId _type;
    Presence _present;
    std::vector<FieldDescriptor> _fields;
    std::vector<Requirement> _required;
};

/**
 * @brief Components added with default values whenever a field of another component is set.
 *
 * Pass `required<PhysicsBody>` to FieldRegistry::add.
 */
template <typename... Components> struct RequiredComponents {};

/** @brief The RequiredComponents value for @p Components. */
template <typename... Components> inline constexpr RequiredComponents<Components...> required{};

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

    /** @param expected, actual Type descriptions such as "list of string". */
    FieldKindMismatch(std::string_view component, std::string_view field, std::string_view expected,
                      std::string_view actual);
};

/** @brief Thrown when a field is given a value its component cannot hold. */
class ENGINE_CORE_EXPORT FieldValueOutOfRange : public std::out_of_range {
public:
    FieldValueOutOfRange(std::string_view component, std::string_view field,
                         std::string_view reason);

    /** @return Why the value was rejected, without the field name, e.g. "size must be positive". */
    [[nodiscard]] const std::string& reason() const noexcept { return _reason; }

private:
    std::string _reason;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace detail {

template <typename Component, typename Read, typename Write>
[[nodiscard]] FieldDescriptor make_field(std::string name, FieldKind kind,
                                         std::optional<FieldKind> element, FieldFlags flags,
                                         Read read, Write write) {
    return FieldDescriptor{std::move(name),
                           kind,
                           flags,
                           [read](EntityManager& world, Entity entity) -> FieldValue {
                               return read(std::as_const(world.get<Component>(entity)));
                           },
                           [write](EntityManager& world, Entity entity, const FieldValue& value) {
                               Component component = world.has<Component>(entity)
                                                         ? world.get<Component>(entity)
                                                         : Component{};
                               write(component, value);
                               world.add(entity, std::move(component));
                           },
                           element};
}

} // namespace detail

/**
 * @brief Builds a field from accessors working on the concrete component.
 *
 * @tparam Component Component type the field belongs to.
 * @param read Callable `FieldValue(const Component&)`.
 * @param write Callable `void(Component&, const FieldValue&)`, given a value of @p kind. It
 * rejects a value by throwing std::out_of_range.
 * @note A throwing @p write leaves the entity unchanged. Use list_field for FieldKind::List.
 */
template <typename Component, typename Read, typename Write>
[[nodiscard]] FieldDescriptor component_field(std::string name, FieldKind kind, FieldFlags flags,
                                              Read read, Write write) {
    return detail::make_field<Component>(std::move(name), kind, std::nullopt, flags,
                                         std::move(read), std::move(write));
}

/**
 * @brief Builds a list field from accessors working on the concrete component.
 *
 * @tparam Component Component type the field belongs to.
 * @param element Kind of every item; FieldRegistry::add rejects FieldKind::List.
 * @param read Callable `FieldList(const Component&)`.
 * @param write Callable `void(Component&, const FieldList&)`, given a list of @p element items.
 * It rejects a value by throwing std::out_of_range.
 * @note A throwing @p write leaves the entity unchanged.
 */
template <typename Component, typename Read, typename Write>
[[nodiscard]] FieldDescriptor list_field(std::string name, FieldKind element, FieldFlags flags,
                                         Read read, Write write) {
    return detail::make_field<Component>(
        std::move(name), FieldKind::List, element, flags,
        [read](const Component& component) { return FieldValue{read(component)}; },
        [write](Component& component, const FieldValue& value) {
            write(component, std::get<FieldList>(value));
        });
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
     * @throws std::invalid_argument If a name is empty, a field has no getter or setter, or a
     * list field has no scalar item kind.
     */
    template <typename Component> void add(std::string name, std::vector<FieldDescriptor> fields) {
        add<Component>(std::move(name), std::move(fields), required<>);
    }

    /**
     * @brief Registers a component type whose fields need other components to work.
     *
     * Setting any field of @p Component also adds every one of @p Required the entity lacks,
     * with default values, as a collider needs a body to be simulated.
     *
     * @note Throws everything the other overload throws.
     */
    template <typename Component, typename... Required>
    void add(std::string name, std::vector<FieldDescriptor> fields,
             RequiredComponents<Required...>) {
        add(ComponentDescriptor{
            std::move(name),
            component_type_id<Component>(),
            &owns<Component>,
            std::move(fields),
            {ComponentDescriptor::Requirement{.registered = &is_known<Required>,
                                              .ensure = &ensure<Required>,
                                              .type_name = typeid(Required).name()}...}});
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
     * Also adds the components the field's component requires.
     *
     * @throws FieldKindMismatch If @p value is not of the field's kind, or is a list holding
     * items of another kind; the entity is unchanged.
     * @throws FieldValueOutOfRange If the component cannot hold @p value; the entity is unchanged.
     * @throws ComponentNotRegistered If the world does not know the component type or a
     * component it requires; the entity is unchanged.
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

    template <typename Component> static bool is_known(const EntityManager& world) {
        return world.is_registered<Component>();
    }

    template <typename Component> static void ensure(EntityManager& world, Entity entity) {
        if (!world.has<Component>(entity)) {
            world.add(entity, Component{});
        }
    }

    ENGINE_CORE_EXPORT void add(ComponentDescriptor component);
    [[nodiscard]] FieldRef require(std::string_view component, std::string_view field) const;

    std::vector<ComponentDescriptor> _components;
};

/** @return The labels of @p tags as a list of strings, in insertion order. */
[[nodiscard]] ENGINE_CORE_EXPORT FieldList labels_to_list(const Tags& tags);

/**
 * @return Tags holding every string of @p list once, in order.
 * @throws std::bad_variant_access If an item is not a string.
 */
[[nodiscard]] ENGINE_CORE_EXPORT Tags labels_from_list(const FieldList& list);

/**
 * @brief Registers the fields of the engine-core components: Position, Sprite and Tags.
 *
 * @throws DuplicateComponentName If one of them is already registered.
 */
ENGINE_CORE_EXPORT void register_core_fields(FieldRegistry& registry);

} // namespace engine::core
