#include "text/TextEdit.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "text/Utf8.hpp"

namespace editor {

TextEdit::TextEdit(std::string text) : _text(std::move(text)), _caret(_text.size()), _anchor(_caret) {}

const std::string& TextEdit::text() const noexcept { return _text; }

std::size_t TextEdit::caret() const noexcept { return _caret; }

std::size_t TextEdit::anchor() const noexcept { return _anchor; }

bool TextEdit::has_selection() const noexcept { return _caret != _anchor; }

std::string_view TextEdit::selection() const {
    const std::size_t first = std::min(_caret, _anchor);
    const std::size_t last = std::max(_caret, _anchor);
    return std::string_view(_text).substr(first, last - first);
}

void TextEdit::insert(std::string_view typed) {
    erase_selection();
    _text.insert(_caret, typed);
    _caret += typed.size();
    _anchor = _caret;
}

void TextEdit::erase_backward() {
    if (!has_selection()) {
        _anchor = previous_boundary(_text, _caret);
    }
    erase_selection();
}

void TextEdit::erase_forward() {
    if (!has_selection()) {
        _anchor = next_boundary(_text, _caret);
    }
    erase_selection();
}

void TextEdit::erase_word_backward() {
    if (!has_selection()) {
        _anchor = previous_word_start(_text, _caret);
    }
    erase_selection();
}

void TextEdit::erase_word_forward() {
    if (!has_selection()) {
        _anchor = next_word_end(_text, _caret);
    }
    erase_selection();
}

void TextEdit::move(Motion motion, bool extend) {
    const bool collapses = !extend && has_selection();
    if (collapses && motion == Motion::left) {
        place_caret(std::min(_caret, _anchor), false);
        return;
    }
    if (collapses && motion == Motion::right) {
        place_caret(std::max(_caret, _anchor), false);
        return;
    }
    place_caret(target_of(motion), extend);
}

void TextEdit::place_caret(std::size_t index, bool extend) {
    _caret = floor_boundary(_text, index);
    if (!extend) {
        _anchor = _caret;
    }
}

void TextEdit::select_all() {
    _anchor = 0;
    _caret = _text.size();
}

void TextEdit::select_word() {
    _anchor = word_start_at(_text, _caret);
    _caret = word_end_at(_text, _caret);
    if (_anchor == _caret) {
        _caret = next_boundary(_text, _caret);
    }
}

std::size_t TextEdit::target_of(Motion motion) const {
    switch (motion) {
    case Motion::left:
        return previous_boundary(_text, _caret);
    case Motion::right:
        return next_boundary(_text, _caret);
    case Motion::word_left:
        return previous_word_start(_text, _caret);
    case Motion::word_right:
        return next_word_end(_text, _caret);
    case Motion::home:
        return 0;
    case Motion::end:
        return _text.size();
    }
    return _caret;
}

void TextEdit::erase_selection() {
    const std::size_t first = std::min(_caret, _anchor);
    const std::size_t last = std::max(_caret, _anchor);
    _text.erase(first, last - first);
    _caret = first;
    _anchor = first;
}

std::size_t caret_at(std::string_view text, float x, const MeasureWidth& measure) {
    std::size_t best = 0;
    float best_distance = std::abs(x);
    while (best < text.size()) {
        const std::size_t index = next_boundary(text, best);
        const float distance = std::abs(measure(text.substr(0, index)) - x);
        if (distance >= best_distance) {
            break;
        }
        best = index;
        best_distance = distance;
    }
    return best;
}

} // namespace editor
