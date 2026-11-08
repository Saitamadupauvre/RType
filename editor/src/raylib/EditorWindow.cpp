#include "raylib/EditorWindow.hpp"

#include <clay.h>
#include <GLFW/glfw3.h>
#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "clay/ClayContext.hpp"
#include "raylib/ClayRenderer.hpp"
#include "raylib/ViewportPainter.hpp"
#include "ui/ClickCounter.hpp"
#include "ui/Layout.hpp"
#include "ui/LetterKey.hpp"
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

constexpr std::array punctuation_keys{
    KEY_APOSTROPHE, KEY_COMMA,        KEY_MINUS,     KEY_PERIOD,        KEY_SLASH, KEY_SEMICOLON,
    KEY_EQUAL,      KEY_LEFT_BRACKET, KEY_BACKSLASH, KEY_RIGHT_BRACKET, KEY_GRAVE,
};

constexpr std::array named_keys{
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

void add_if_letter(std::vector<std::pair<int, Key>>& letters, int code) {
    const char* name = glfwGetKeyName(code, 0);
    if (const std::optional<Key> key = letter_key(name != nullptr ? name : "")) {
        letters.emplace_back(code, *key);
    }
}

std::vector<std::pair<int, Key>> layout_letters() {
    std::vector<std::pair<int, Key>> letters;
    for (int code = KEY_A; code <= KEY_Z; ++code) {
        add_if_letter(letters, code);
    }
    for (const int code : punctuation_keys) {
        add_if_letter(letters, code);
    }
    return letters;
}

std::vector<Key> keys_where(const std::vector<std::pair<int, Key>>& letters, bool (*test)(int)) {
    std::vector<Key> keys;
    for (const auto& [code, key] : letters) {
        if (test(code)) {
            keys.push_back(key);
        }
    }
    for (const auto& [code, key] : named_keys) {
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
    const std::vector<std::pair<int, Key>> letters = layout_letters();
    const Clay_Vector2 pointer = to_clay(GetMousePosition());
    const bool pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    return {.pointer = pointer,
            .primary_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT),
            .middle_down = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE),
            .primary_pressed = pressed,
            .click_count = pressed ? clicks.press(pointer, GetTime()) : std::uint8_t{1},
            .wheel = to_clay(GetMouseWheelMoveV()),
            .delta_time = GetFrameTime(),
            .keys_pressed = keys_where(letters, IsKeyPressed),
            .keys_repeated = keys_where(letters, IsKeyPressedRepeat),
            .typed = typed_text(),
            .primary_modifier = IsKeyDown(primary_left) || IsKeyDown(primary_right),
            .shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT),
            .alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)};
}

void toggle_debug_view() { Clay_SetDebugModeEnabled(!Clay_IsDebugModeEnabled()); }

void run_frames(Editor& editor) {
    ClayRenderer renderer(font_directory());
    const MeasureText measure = [&renderer](auto text, const auto& config) {
        return renderer.measure(text, config);
    };
    const ClayContext clay(window_size(), measure);
    ViewportPainter painter(editor.session().game() / "assets");
    renderer.set_custom_painter([&painter](const Clay_RenderCommand& command) { painter.paint(command); });
    const Platform platform{.measure = measure,
                            .read_clipboard = read_clipboard,
                            .write_clipboard = write_clipboard,
                            .texture_size = [&painter](std::string_view path) {
                                return painter.texture_size(path);
                            }};
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
        editor.update();
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
