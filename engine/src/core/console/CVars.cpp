#include "engine/core/console/CVars.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <format>
#include <limits>
#include <system_error>
#include <type_traits>
#include <utility>

namespace engine::core::console {

namespace {

CVarResult failure(std::string error) { return {.ok = false, .error = std::move(error)}; }

bool valid_name(std::string_view name) {
    if (name.empty() || name.front() < 'a' || name.front() > 'z') {
        return false;
    }
    return std::ranges::all_of(name, [](char letter) {
        return (letter >= 'a' && letter <= 'z') || (letter >= '0' && letter <= '9') ||
               letter == '_' || letter == '.';
    });
}

bool numeric(CVarKind kind) { return kind == CVarKind::integer || kind == CVarKind::number; }

std::optional<double> number_of(const CVarValue& value) {
    if (const auto* integer = std::get_if<std::int64_t>(&value)) {
        return static_cast<double>(*integer);
    }
    if (const auto* number = std::get_if<double>(&value)) {
        return *number;
    }
    return std::nullopt;
}

bool whole(double number) {
    constexpr auto limit = static_cast<double>(std::numeric_limits<std::int64_t>::max());
    return std::isfinite(number) && std::trunc(number) == number && std::abs(number) < limit;
}

std::optional<CVarValue> convert(const CVarValue& value, CVarKind kind) {
    if (kind_of(value) == kind) {
        return value;
    }
    if (kind == CVarKind::number) {
        if (const auto* integer = std::get_if<std::int64_t>(&value)) {
            return static_cast<double>(*integer);
        }
    }
    if (kind == CVarKind::integer) {
        if (const auto* number = std::get_if<double>(&value); number != nullptr && whole(*number)) {
            return static_cast<std::int64_t>(*number);
        }
    }
    return std::nullopt;
}

std::string lowercase(std::string_view text) {
    std::string lowered(text);
    std::ranges::transform(lowered, lowered.begin(), [](char letter) {
        return letter >= 'A' && letter <= 'Z' ? static_cast<char>(letter - 'A' + 'a') : letter;
    });
    return lowered;
}

std::optional<bool> parse_bool(std::string_view text) {
    const std::string lowered = lowercase(text);
    if (lowered == "true" || lowered == "on" || lowered == "yes" || lowered == "1") {
        return true;
    }
    if (lowered == "false" || lowered == "off" || lowered == "no" || lowered == "0") {
        return false;
    }
    return std::nullopt;
}

std::optional<std::int64_t> parse_integer(std::string_view text) {
    std::int64_t parsed{};
    const char* end = text.data() + text.size();
    const auto [stop, error] = std::from_chars(text.data(), end, parsed);
    if (error != std::errc{} || stop != end || text.empty()) {
        return std::nullopt;
    }
    return parsed;
}

std::optional<double> parse_double(std::string_view text) {
    const std::string copy(text);
    if (copy.empty() || copy.front() == ' ') {
        return std::nullopt;
    }
    char* stop = nullptr;
    const double parsed = std::strtod(copy.c_str(), &stop);
    if (stop != copy.c_str() + copy.size() || !std::isfinite(parsed)) {
        return std::nullopt;
    }
    return parsed;
}

std::optional<CVarValue> parse(std::string_view text, CVarKind kind) {
    switch (kind) {
    case CVarKind::boolean:
        if (const auto parsed = parse_bool(text)) {
            return *parsed;
        }
        return std::nullopt;
    case CVarKind::integer:
        if (const auto parsed = parse_integer(text)) {
            return *parsed;
        }
        return std::nullopt;
    case CVarKind::number:
        if (const auto parsed = parse_double(text)) {
            return *parsed;
        }
        return std::nullopt;
    case CVarKind::string:
        return std::string(text);
    }
    return std::nullopt;
}

CVarResult check_range(const std::string& name, const CVarValue& value,
                       const std::optional<CVarRange>& range) {
    const auto number = number_of(value);
    if (number && !std::isfinite(*number)) {
        return failure(std::format("{} must be a finite number", name));
    }
    if (!range || !number || (*number >= range->min && *number <= range->max)) {
        return {};
    }
    return failure(std::format("{} must be between {} and {}", name, range->min, range->max));
}

CVarResult check_spec(const CVarSpec& spec) {
    if (!valid_name(spec.name)) {
        return failure(std::format("'{}' is not a valid name: use lowercase letters, digits, '_' "
                                   "and '.', starting with a letter",
                                   spec.name));
    }
    if (!spec.range) {
        return check_range(spec.name, spec.default_value, std::nullopt);
    }
    if (!numeric(kind_of(spec.default_value))) {
        return failure(std::format("{} is a {} and cannot have a range", spec.name,
                                   kind_name(kind_of(spec.default_value))));
    }
    if (!std::isfinite(spec.range->min) || !std::isfinite(spec.range->max) ||
        spec.range->min > spec.range->max) {
        return failure(std::format("{} has an empty range", spec.name));
    }
    return check_range(spec.name, spec.default_value, spec.range);
}

} // namespace

CVarKind kind_of(const CVarValue& value) noexcept {
    return static_cast<CVarKind>(static_cast<std::uint8_t>(value.index()));
}

std::string_view kind_name(CVarKind kind) noexcept {
    switch (kind) {
    case CVarKind::boolean:
        return "bool";
    case CVarKind::integer:
        return "int";
    case CVarKind::number:
        return "number";
    case CVarKind::string:
        return "string";
    }
    return "unknown";
}

std::string to_string(const CVarValue& value) {
    return std::visit(
        [](const auto& held) -> std::string {
            using Held = std::decay_t<decltype(held)>;
            if constexpr (std::is_same_v<Held, bool>) {
                return held ? "true" : "false";
            } else if constexpr (std::is_same_v<Held, std::string>) {
                return held;
            } else {
                return std::format("{}", held);
            }
        },
        value);
}

CVarResult CVars::add(CVarSpec spec) { return add(std::move(spec), Listener{}); }

CVarResult CVars::add(CVarSpec spec, Listener on_change) {
    if (auto checked = check_spec(spec); !checked) {
        return checked;
    }
    if (contains(spec.name)) {
        return failure(std::format("{} is already registered", spec.name));
    }
    Entry entry{.info = {.name = spec.name,
                         .kind = kind_of(spec.default_value),
                         .value = spec.default_value,
                         .default_value = spec.default_value,
                         .description = std::move(spec.description),
                         .range = spec.range},
                .listeners = {}};
    if (on_change) {
        entry.listeners.push_back(std::move(on_change));
    }
    _entries.emplace(std::move(spec.name), std::move(entry));
    return {};
}

CVarResult CVars::set(std::string_view name, const CVarValue& value) {
    const auto found = _entries.find(name);
    if (found == _entries.end()) {
        return failure(std::format("unknown variable {}", name));
    }
    CVarInfo& info = found->second.info;
    const auto converted = convert(value, info.kind);
    if (!converted) {
        return failure(std::format("{} expects a {}, not a {}", info.name, kind_name(info.kind),
                                   kind_name(kind_of(value))));
    }
    if (auto checked = check_range(info.name, *converted, info.range); !checked) {
        return checked;
    }
    if (*converted == info.value) {
        return {};
    }
    info.value = *converted;
    const CVarValue changed = info.value;
    const std::vector<Listener> listeners = found->second.listeners;
    for (const auto& listener : listeners) {
        listener(changed);
    }
    return {};
}

CVarResult CVars::set_from_string(std::string_view name, std::string_view text) {
    const auto found = _entries.find(name);
    if (found == _entries.end()) {
        return failure(std::format("unknown variable {}", name));
    }
    const CVarKind kind = found->second.info.kind;
    const auto parsed = parse(text, kind);
    if (!parsed) {
        return failure(std::format("{} expects a {}, '{}' is not one", found->second.info.name,
                                   kind_name(kind), text));
    }
    return set(name, *parsed);
}

CVarResult CVars::reset(std::string_view name) {
    const auto found = _entries.find(name);
    if (found == _entries.end()) {
        return failure(std::format("unknown variable {}", name));
    }
    return set(name, CVarValue(found->second.info.default_value));
}

std::optional<CVarValue> CVars::get(std::string_view name) const {
    const auto found = _entries.find(name);
    if (found == _entries.end()) {
        return std::nullopt;
    }
    return found->second.info.value;
}

std::optional<CVarInfo> CVars::find(std::string_view name) const {
    const auto found = _entries.find(name);
    if (found == _entries.end()) {
        return std::nullopt;
    }
    return found->second.info;
}

std::vector<CVarInfo> CVars::list() const {
    std::vector<CVarInfo> infos;
    infos.reserve(_entries.size());
    for (const auto& [name, entry] : _entries) {
        infos.push_back(entry.info);
    }
    return infos;
}

bool CVars::contains(std::string_view name) const { return _entries.contains(name); }

bool CVars::get_bool(std::string_view name, bool fallback) const {
    const auto value = get(name);
    const bool* held = value ? std::get_if<bool>(&*value) : nullptr;
    return held != nullptr ? *held : fallback;
}

double CVars::get_number(std::string_view name, double fallback) const {
    const auto value = get(name);
    const auto number = value ? number_of(*value) : std::nullopt;
    return number.value_or(fallback);
}

} // namespace engine::core::console
