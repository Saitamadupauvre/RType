#include "raylib/EditorWindow.hpp"

#include <clay.h>
#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "clay/ClayContext.hpp"
#include "raylib/ClayRenderer.hpp"
#include "ui/ClickCounter.hpp"
#include "ui/Layout.hpp"
#include "ui/Platform.hpp"
#include "ui/Theme.hpp"
#include "ui/Ui.hpp"
#include "ui/UiInput.hpp"
#include "ui/WidgetState.hpp"

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
    std::pair{KEY_A, Key::a},
    std::pair{KEY_C, Key::c},
    std::pair{KEY_S, Key::s},
    std::pair{KEY_V, Key::v},
    std::pair{KEY_X, Key::x},
    std::pair{KEY_Y, Key::y},
    std::pair{KEY_Z, Key::z},
    std::pair{KEY_G, Key::g},
    std::pair{KEY_H, Key::h},
    std::pair{KEY_DELETE, Key::del},
    std::pair{KEY_BACKSPACE, Key::backspace},
    std::pair{KEY_ENTER, Key::enter},
    std::pair{KEY_KP_ENTER, Key::enter},
    std::pair{KEY_ESCAPE, Key::escape},
    std::pair{KEY_TAB, Key::tab},
    std::pair{KEY_LEFT, Key::left},
    std::pair{KEY_RIGHT, Key::right},
    std::pair{KEY_UP, Key::up},
    std::pair{KEY_DOWN, Key::down},
    std::pair{KEY_HOME, Key::home},
    std::pair{KEY_END, Key::end},
};

#ifdef __APPLE__
constexpr KeyboardKey primary_left = KEY_LEFT_SUPER;
constexpr KeyboardKey primary_right = KEY_RIGHT_SUPER;
#else
constexpr KeyboardKey primary_left = KEY_LEFT_CONTROL;
constexpr KeyboardKey primary_right = KEY_RIGHT_CONTROL;
#endif

std::vector<Key> keys_where(bool (*test)(int)) {
    std::vector<Key> keys;
    for (const auto& [code, key] : key_map) {
        if (test(code)) {
            keys.push_back(key);
        }
    }
    return keys;
}

std::string typed_text() {
    std::string typed;
    for (int codepoint = GetCharPressed(); codepoint != 0; codepoint = GetCharPressed()) {
        int size = 0;
        const char* bytes = CodepointToUTF8(codepoint, &size);
        typed.append(bytes, static_cast<std::size_t>(size));
    }
    return typed;
}

std::string read_clipboard() {
    const char* text = GetClipboardText();
    return text != nullptr ? std::string(text) : std::string();
}

void write_clipboard(std::string_view text) { SetClipboardText(std::string(text).c_str()); }

Clay_Vector2 to_clay(Vector2 vector) { return {.x = vector.x, .y = vector.y}; }

UiInput read_input(ClickCounter& clicks) {
    const Clay_Vector2 pointer = to_clay(GetMousePosition());
    const bool pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    return {.pointer = pointer,
            .primary_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT),
            .primary_pressed = pressed,
            .click_count = pressed ? clicks.press(pointer, GetTime()) : std::uint8_t{1},
            .wheel = to_clay(GetMouseWheelMoveV()),
            .delta_time = GetFrameTime(),
            .keys_pressed = keys_where(IsKeyPressed),
            .keys_repeated = keys_where(IsKeyPressedRepeat),
            .typed = typed_text(),
            .primary_modifier = IsKeyDown(primary_left) || IsKeyDown(primary_right),
            .shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)};
}

void toggle_debug_view() { Clay_SetDebugModeEnabled(!Clay_IsDebugModeEnabled()); }

void run_frames(Editor& editor) {
    ClayRenderer renderer(font_directory());
    const MeasureText measure = [&renderer](auto text, const auto& config) {
        return renderer.measure(text, config);
    };
    const ClayContext clay(window_size(), measure);
    const Platform platform{
        .measure = measure, .read_clipboard = read_clipboard, .write_clipboard = write_clipboard};
    WidgetState widgets;
    ClickCounter clicks;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_F11)) {
            toggle_debug_view();
        }
        Ui ui(Theme{.scale = GetWindowScaleDPI().x}, read_input(clicks), widgets, platform);
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
