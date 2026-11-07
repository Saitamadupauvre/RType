#include "ui/widgets/NumberField.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

#include "ui/widgets/TextField.hpp"

namespace editor {

namespace {

std::optional<double> parse(const std::string& text) {
    if (text.empty()) {
        return std::nullopt;
    }
    char* end = nullptr;
    errno = 0;
    const double parsed = std::strtod(text.c_str(), &end);
    if (errno != 0 || end != text.c_str() + text.size() || !std::isfinite(parsed)) {
        return std::nullopt;
    }
    return parsed;
}

double clamp(double value, const NumberFieldOptions& options) {
    return std::clamp(value, options.min.value_or(std::numeric_limits<double>::lowest()),
                      options.max.value_or(std::numeric_limits<double>::max()));
}

template <typename Number> std::string format(Number value, const NumberFieldOptions& options) {
    if constexpr (std::is_integral_v<Number>) {
        return std::to_string(value);
    } else {
        return std::format("{:.{}f}", value, std::max(options.decimals, 0));
    }
}

template <typename Number>
std::optional<Number> to_number(const std::string& text, const NumberFieldOptions& options) {
    const std::optional<double> parsed = parse(text);
    if (!parsed) {
        return std::nullopt;
    }
    const auto low = static_cast<double>(std::numeric_limits<Number>::lowest());
    const auto high = static_cast<double>(std::numeric_limits<Number>::max());
    double number = std::clamp(clamp(*parsed, options), low, high);
    if constexpr (std::is_integral_v<Number>) {
        number = std::round(number);
        if (number >= high) {
            return std::numeric_limits<Number>::max();
        }
    }
    return static_cast<Number>(number);
}

template <typename Number>
FieldResult<Number> edit_number(Ui& ui, Clay_ElementId id, Number value,
                                const NumberFieldOptions& options) {
    const Clay_String shown = ui.keep(format(value, options));
    const TextResult text =
        text_field(ui, id, {shown.chars, static_cast<std::size_t>(shown.length)});
    if (text.event != FieldEvent::committed) {
        return {.event = text.event, .value = value};
    }
    if (text.value == std::string_view(shown.chars, static_cast<std::size_t>(shown.length))) {
        return {.event = FieldEvent::committed, .value = value};
    }
    const std::optional<Number> number = to_number<Number>(text.value, options);
    if (!number) {
        return {.event = FieldEvent::rejected, .value = value};
    }
    return {.event = FieldEvent::committed, .value = *number};
}

} // namespace

FieldResult<float> number_field(Ui& ui, Clay_ElementId id, float value,
                                const NumberFieldOptions& options) {
    return edit_number(ui, id, value, options);
}

FieldResult<int> number_field(Ui& ui, Clay_ElementId id, int value,
                              const NumberFieldOptions& options) {
    return edit_number(ui, id, value, options);
}

FieldResult<std::int64_t> number_field(Ui& ui, Clay_ElementId id, std::int64_t value,
                                       const NumberFieldOptions& options) {
    return edit_number(ui, id, value, options);
}

} // namespace editor
