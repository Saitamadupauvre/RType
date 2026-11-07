#include "ui/widgets/FieldKeys.hpp"

#include <array>
#include <string>
#include <utility>

namespace editor {

namespace {

void apply_clipboard(const UiInput& input, const Platform& platform, TextEdit& draft) {
    if (pressed_or_repeated(input, Key::a)) {
        draft.select_all();
    }
    if (pressed_or_repeated(input, Key::c) && draft.has_selection()) {
        platform.write_clipboard(draft.selection());
    }
    if (pressed_or_repeated(input, Key::x) && draft.has_selection()) {
        platform.write_clipboard(draft.selection());
        draft.insert({});
    }
    if (pressed_or_repeated(input, Key::v)) {
        draft.insert(platform.read_clipboard());
    }
}

void apply_motions(const UiInput& input, TextEdit& draft) {
    const bool word = input.primary_modifier;
    const std::array motions{
        std::pair{Key::left, word ? Motion::word_left : Motion::left},
        std::pair{Key::right, word ? Motion::word_right : Motion::right},
        std::pair{Key::home, Motion::home},
        std::pair{Key::end, Motion::end},
    };
    for (const auto& [key, motion] : motions) {
        if (pressed_or_repeated(input, key)) {
            draft.move(motion, input.shift);
        }
    }
}

void apply_erase(const UiInput& input, TextEdit& draft) {
    const bool word = input.primary_modifier;
    if (pressed_or_repeated(input, Key::backspace)) {
        word ? draft.erase_word_backward() : draft.erase_backward();
    }
    if (pressed_or_repeated(input, Key::del)) {
        word ? draft.erase_word_forward() : draft.erase_forward();
    }
}

} // namespace

void apply_editing_keys(const UiInput& input, const Platform& platform, TextEdit& draft) {
    if (input.primary_modifier) {
        apply_clipboard(input, platform, draft);
    } else if (!input.typed.empty()) {
        draft.insert(input.typed);
    }
    apply_motions(input, draft);
    apply_erase(input, draft);
}

} // namespace editor
