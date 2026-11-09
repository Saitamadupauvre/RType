#pragma once

#include <clay.h>

#include <string_view>

inline Clay_Dimensions fixed_measure(std::string_view text, const Clay_TextElementConfig& config) {
    return {.width = static_cast<float>(text.size()) * static_cast<float>(config.fontSize) / 2.F,
            .height = static_cast<float>(config.fontSize)};
}
