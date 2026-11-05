#include "clay/ClayContext.hpp"

#include <cstddef>
#include <utility>

namespace editor {

ClayContext::ClayContext(Clay_Dimensions size, MeasureText measure)
    : _memory(Clay_MinMemorySize()), _measure(std::move(measure)) {
    const Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(_memory.size(), _memory.data());
    Clay_Initialize(arena, size, {.errorHandlerFunction = &report_error, .userData = this});
    Clay_SetMeasureTextFunction(&measure_text, this);
}

ClayContext::~ClayContext() { Clay_SetCurrentContext(nullptr); }

const std::vector<std::string>& ClayContext::errors() const { return _errors; }

Clay_Dimensions ClayContext::measure_text(Clay_StringSlice text, Clay_TextElementConfig* config,
                                          void* context) {
    const auto& self = *static_cast<ClayContext*>(context);
    return self._measure({text.chars, static_cast<std::size_t>(text.length)}, *config);
}

void ClayContext::report_error(Clay_ErrorData error) {
    auto& self = *static_cast<ClayContext*>(error.userData);
    self._errors.emplace_back(error.errorText.chars,
                              static_cast<std::size_t>(error.errorText.length));
}

} // namespace editor
