#pragma once

#include <clay.h>

#include <cstdint>

namespace editor {

namespace color {

inline constexpr Clay_Color menu_bar{.r = 21, .g = 23, .b = 27, .a = 255};
inline constexpr Clay_Color toolbar{.r = 31, .g = 34, .b = 39, .a = 255};
inline constexpr Clay_Color panel{.r = 35, .g = 38, .b = 45, .a = 255};
inline constexpr Clay_Color viewport{.r = 18, .g = 20, .b = 25, .a = 255};
inline constexpr Clay_Color border{.r = 51, .g = 55, .b = 63, .a = 255};
inline constexpr Clay_Color field{.r = 22, .g = 24, .b = 28, .a = 255};
inline constexpr Clay_Color text{.r = 230, .g = 232, .b = 235, .a = 255};
inline constexpr Clay_Color text_soft{.r = 201, .g = 206, .b = 214, .a = 255};
inline constexpr Clay_Color text_muted{.r = 154, .g = 161, .b = 172, .a = 255};
inline constexpr Clay_Color text_dim{.r = 139, .g = 146, .b = 156, .a = 255};
inline constexpr Clay_Color text_on_accent{.r = 27, .g = 29, .b = 34, .a = 255};
inline constexpr Clay_Color accent{.r = 240, .g = 160, .b = 75, .a = 255};
inline constexpr Clay_Color focus{.r = 76, .g = 154, .b = 255, .a = 255};
inline constexpr Clay_Color selected{.r = 47, .g = 58, .b = 77, .a = 255};

} // namespace color

namespace size {

inline constexpr float menu_bar = 36.F;
inline constexpr float toolbar = 40.F;
inline constexpr float sidebar = 260.F;
inline constexpr float inspector = 340.F;
inline constexpr float bottom_panel = 150.F;
inline constexpr float tabs = 30.F;
inline constexpr float status_bar = 24.F;
inline constexpr float panel_header = 32.F;
inline constexpr float button = 26.F;
inline constexpr float tool_button = 28.F;
inline constexpr float separator = 20.F;
inline constexpr float text = 16.F;
inline constexpr float small_text = 13.F;
inline constexpr float gutter = 12.F;
inline constexpr float radius = 6.F;

} // namespace size

enum class Face : std::uint8_t { regular, bold };

struct Theme {
    float scale = 1.F;

    [[nodiscard]] float px(float design) const;
    [[nodiscard]] std::uint16_t px16(float design) const;
};

} // namespace editor
