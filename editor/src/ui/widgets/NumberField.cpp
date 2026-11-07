#include "ui/widgets/NumberField.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <format>
#include <limits>
#include <string>
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

std::string format(float value, const NumberFieldOptions& options) {
    return std::format("{:.{}f}", value, std::max(options.decimals, 0));
}

std::string format(int value, const NumberFieldOptions&) {
    return std::to_string(value);
}

template <typename Number>
std::optional<Number> to_number(const std::string& text, const NumberFieldOptions& options) {
    const std::optional<double> parsed = parse(text);
    if (!parsed) {
        return std::nullopt;
    }
    const double low = std::numeric_limits<Number>::lowest();
    const double high = std::numeric_limits<Number>::max();
    double number = std::clamp(clamp(*parsed, options), low, high);
    if constexpr (std::is_integral_v<Number>) {
        number = std::round(number);
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
    const std::optional<Number> number = to_number<Number>(text.value, options);
    if (!number) {
        return {.event = FieldEvent::cancelled, .value = value};
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

} // namespace editor
