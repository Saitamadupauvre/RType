#pragma once

#include <clay.h>

namespace editor {

struct UiInput {
    Clay_Vector2 pointer{};
    bool primary_down = false;
    bool primary_pressed = false;
    Clay_Vector2 wheel{};
    float delta_time = 0.F;
};

} // namespace editor
