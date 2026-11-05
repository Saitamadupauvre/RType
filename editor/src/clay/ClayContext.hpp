#pragma once

#include <clay.h>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

using MeasureText =
    std::function<Clay_Dimensions(std::string_view text, const Clay_TextElementConfig& config)>;

class ClayContext {
public:
    ClayContext(Clay_Dimensions size, MeasureText measure);
    ~ClayContext();

    ClayContext(const ClayContext&) = delete;
    ClayContext& operator=(const ClayContext&) = delete;
    ClayContext(ClayContext&&) = delete;
    ClayContext& operator=(ClayContext&&) = delete;

    [[nodiscard]] const std::vector<std::string>& errors() const;

private:
    static Clay_Dimensions measure_text(Clay_StringSlice text, Clay_TextElementConfig* config,
                                        void* context);
    static void report_error(Clay_ErrorData error);

    std::vector<char> _memory;
    MeasureText _measure;
    std::vector<std::string> _errors;
};

} // namespace editor
