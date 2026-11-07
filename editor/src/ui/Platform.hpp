#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "clay/ClayContext.hpp"

namespace editor {

/** @brief Services the widgets need from the platform layer, given as functions so tests can fake them. */
struct Platform {
    MeasureText measure;
    std::function<std::string()> read_clipboard;
    std::function<void(std::string_view)> write_clipboard;
};

} // namespace editor
