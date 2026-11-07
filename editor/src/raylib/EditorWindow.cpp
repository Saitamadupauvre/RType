#include "raylib/EditorWindow.hpp"

#include <clay.h>
#include <raylib.h>

#include <array>
#include <filesystem>
#include <utility>
#include <vector>

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

constexpr std::array key_map{
    std::pair{KEY_S, Key::s},        std::pair{KEY_Y, Key::y}, std::pair{KEY_Z, Key::z},
    std::pair{KEY_V, Key::v},        std::pair{KEY_G, Key::g}, std::pair{KEY_H, Key::h},
    std::pair{KEY_DELETE, Key::del},
};

#ifdef __APPLE__
constexpr KeyboardKey primary_left = KEY_LEFT_SUPER;
constexpr KeyboardKey primary_right = KEY_RIGHT_SUPER;
#else
constexpr KeyboardKey primary_left = KEY_LEFT_CONTROL;
constexpr KeyboardKey primary_right = KEY_RIGHT_CONTROL;
#endif

std::vector<Key> keys_pressed() {
    std::vector<Key> keys;
    for (const auto& [code, key] : key_map) {
        if (IsKeyPressed(code)) {
            keys.push_back(key);
        }
    }
    return keys;
}

Clay_Vector2 to_clay(Vector2 vector) { return {.x = vector.x, .y = vector.y}; }

UiInput read_input() {
    return {.pointer = to_clay(GetMousePosition()),
            .primary_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT),
            .primary_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
            .wheel = to_clay(GetMouseWheelMoveV()),
            .delta_time = GetFrameTime(),
            .keys_pressed = keys_pressed(),
            .primary_modifier = IsKeyDown(primary_left) || IsKeyDown(primary_right),
            .shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)};
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
