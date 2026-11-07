#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "engine/core/Export.hpp"

namespace engine::core::console {

/** @brief Type of the value a console variable holds. */
enum class CVarKind : std::uint8_t { boolean, integer, number, string };

/** @brief Value of a console variable; the alternative matches its CVarKind. */
using CVarValue = std::variant<bool, std::int64_t, double, std::string>;

/** @brief Inclusive bounds of a numeric console variable. */
struct CVarRange {
    double min;
    double max;
};

/** @brief Everything needed to register a console variable. */
struct CVarSpec {
    std::string name;
    CVarValue default_value;
    std::string description{};
    std::optional<CVarRange> range{};
};

/** @brief State of a registered console variable. */
struct CVarInfo {
    std::string name;
    CVarKind kind;
    CVarValue value;
    CVarValue default_value;
    std::string description;
    std::optional<CVarRange> range;
};

/** @brief Outcome of a registry operation, with the reason of a failure. */
struct CVarResult {
    bool ok{true};
    std::string error;

    explicit operator bool() const noexcept { return ok; }
};

/**
 * @brief Returns the kind of a value.
 */
[[nodiscard]] ENGINE_CORE_EXPORT CVarKind kind_of(const CVarValue& value) noexcept;

/**
 * @brief Returns the name of a kind: "bool", "int", "number" or "string".
 */
[[nodiscard]] ENGINE_CORE_EXPORT std::string_view kind_name(CVarKind kind) noexcept;

/**
 * @brief Formats a value the way the console prints it and set_from_string() reads it.
 */
[[nodiscard]] ENGINE_CORE_EXPORT std::string to_string(const CVarValue& value);

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/**
 * @brief Registry of named, typed console variables (CVars) shared by the engine and scripts.
 *
 * Names are made of lowercase letters, digits, '_' and '.', and start with a letter, e.g.
 * "audio.music_volume". A variable keeps the kind of its default value; an optional range bounds
 * numeric variables.
 *
 * @note Not thread safe: use it from the thread that owns it. Listeners run on that thread.
 */
class ENGINE_CORE_EXPORT CVars {
public:
    /** @brief Called with the new value after a variable changed. */
    using Listener = std::function<void(const CVarValue&)>;

    /**
     * @brief Registers a variable holding its default value.
     *
     * @return A failure when the name is invalid or taken, when a range is given to a boolean or
     * string variable, when the range is empty, or when the default is outside the range.
     */
    CVarResult add(CVarSpec spec);

    /**
     * @brief Registers a variable and a listener called on each of its changes.
     *
     * @return The same failures as add(CVarSpec).
     */
    CVarResult add(CVarSpec spec, Listener on_change);

    /**
     * @brief Changes the value of a variable.
     *
     * An integer is accepted by a number variable, and a number holding a whole value by an
     * integer variable. Listeners only run when the value actually changes.
     *
     * @return A failure for an unknown name, a value of another kind, or a value outside the
     * range; the variable then keeps its value.
     */
    CVarResult set(std::string_view name, const CVarValue& value);

    /**
     * @brief Parses text with the kind of the variable, then sets it.
     *
     * Booleans read true/false, on/off, yes/no and 1/0. Strings are taken as is.
     *
     * @return The failures of set(), or a failure when the text does not parse.
     */
    CVarResult set_from_string(std::string_view name, std::string_view text);

    /**
     * @brief Restores the default value of a variable.
     *
     * @return A failure for an unknown name.
     */
    CVarResult reset(std::string_view name);

    /** @return The value of a variable, or nothing for an unknown name. */
    [[nodiscard]] std::optional<CVarValue> get(std::string_view name) const;

    /** @return The state of a variable, or nothing for an unknown name. */
    [[nodiscard]] std::optional<CVarInfo> find(std::string_view name) const;

    /** @return Every variable, sorted by name. */
    [[nodiscard]] std::vector<CVarInfo> list() const;

    /** @return true when a variable has this name. */
    [[nodiscard]] bool contains(std::string_view name) const;

    /** @return The value of a boolean variable, or @p fallback for an unknown or other name. */
    [[nodiscard]] bool get_bool(std::string_view name, bool fallback) const;

    /** @return The value of a numeric variable, or @p fallback for an unknown or other name. */
    [[nodiscard]] double get_number(std::string_view name, double fallback) const;

private:
    struct Entry {
        CVarInfo info;
        std::vector<Listener> listeners;
    };

    std::map<std::string, Entry, std::less<>> _entries;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace engine::core::console
