#include "raylib/EditorWindow.hpp"

#include <clay.h>
#include <raylib.h>

#include <filesystem>

#include "clay/ClayContext.hpp"
#include "raylib/ClayRenderer.hpp"
#include "ui/Layout.hpp"
#include "ui/Theme.hpp"
#include "ui/Ui.hpp"
#include "ui/UiInput.hpp"

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

Clay_Vector2 to_clay(Vector2 vector) { return {.x = vector.x, .y = vector.y}; }

UiInput read_input() {
    return {.pointer = to_clay(GetMousePosition()),
            .primary_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT),
            .primary_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
            .wheel = to_clay(GetMouseWheelMoveV()),
            .delta_time = GetFrameTime()};
}

void toggle_debug_view() { Clay_SetDebugModeEnabled(!Clay_IsDebugModeEnabled()); }

void run_frames(Editor& editor) {
    ClayRenderer renderer(font_directory());
    const ClayContext clay(window_size(), [&renderer](auto text, const auto& config) {
        return renderer.measure(text, config);
    });

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_F11)) {
            toggle_debug_view();
        }
        Ui ui(Theme{.scale = GetWindowScaleDPI().x}, read_input());
        const Clay_RenderCommandArray commands = build_frame(ui, editor, window_size());
        BeginDrawing();
        ClearBackground(BLACK);
        renderer.render(commands);
        EndDrawing();
        for (const Action& action : ui.take_actions()) {
            editor.handle(action);
        }
    }
}

} // namespace

bool run_editor_window(Editor& editor) {
    SetConfigFlags(window_flags);
    InitWindow(initial_width, initial_height, "Editor");
    if (!IsWindowReady()) {
        return false;
    }
    SetExitKey(KEY_NULL);
    run_frames(editor);
    CloseWindow();
    return true;
}

} // namespace editor
