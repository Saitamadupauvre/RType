#include "raylib/EditorWindow.hpp"

#include <clay.h>
#include <raylib.h>

#include <filesystem>

#include "clay/ClayContext.hpp"
#include "raylib/ClayRenderer.hpp"
#include "ui/Layout.hpp"
#include "ui/Theme.hpp"

namespace editor {

namespace {

constexpr int initial_width = 1440;
constexpr int initial_height = 900;
constexpr unsigned int window_flags = static_cast<unsigned int>(FLAG_WINDOW_RESIZABLE) |
                                      static_cast<unsigned int>(FLAG_VSYNC_HINT) |
                                      static_cast<unsigned int>(FLAG_MSAA_4X_HINT);

Clay_Dimensions window_size() {
    return {.width = static_cast<float>(GetScreenWidth()),
            .height = static_cast<float>(GetScreenHeight())};
}

std::filesystem::path font_directory() {
    return std::filesystem::path(GetApplicationDirectory()) / "editor-assets" / "fonts";
}

void toggle_debug_view() { Clay_SetDebugModeEnabled(!Clay_IsDebugModeEnabled()); }

void run_frames() {
    ClayRenderer renderer(font_directory());
    const ClayContext clay(window_size(), [&renderer](auto text, const auto& config) {
        return renderer.measure(text, config);
    });

    while (!WindowShouldClose()) {
        const Theme theme{.scale = GetWindowScaleDPI().x};
        const Vector2 pointer = GetMousePosition();
        const Vector2 wheel = GetMouseWheelMoveV();
        Clay_SetPointerState({.x = pointer.x, .y = pointer.y},
                             IsMouseButtonDown(MOUSE_BUTTON_LEFT));
        Clay_UpdateScrollContainers(false, {.x = wheel.x, .y = wheel.y}, GetFrameTime());

        if (IsKeyPressed(KEY_F11)) {
            toggle_debug_view();
        }

        const Clay_RenderCommandArray commands = build_layout(theme, window_size());
        BeginDrawing();
        ClearBackground(BLACK);
        renderer.render(commands);
        EndDrawing();
    }
}

} // namespace

bool run_editor_window() {
    SetConfigFlags(window_flags);
    InitWindow(initial_width, initial_height, "Editor");
    if (!IsWindowReady()) {
        return false;
    }
    run_frames();
    CloseWindow();
    return true;
}

} // namespace editor
