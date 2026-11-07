#include "ui/WidgetState.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>

namespace editor {

bool WidgetState::wants_keyboard() const noexcept { return _focused.has_value(); }

bool WidgetState::is_focused(Clay_ElementId id) const noexcept { return _focused == id.id; }

std::optional<FocusRequest> WidgetState::focus_requested(Clay_ElementId id) const noexcept {
    if (_pending && _pending->id == id.id) {
        return _pending;
    }
    return std::nullopt;
}

void WidgetState::request_focus(Clay_ElementId id, float click_x) {
    _requested = FocusRequest{.id = id.id, .click_x = click_x};
}

void WidgetState::focus(Clay_ElementId id, std::string_view value) {
    _focused = id.id;
    _draft = TextEdit(std::string(value));
    _original = std::string(value);
    _text_offset = 0.F;
}

float& WidgetState::text_offset() noexcept { return _text_offset; }

void WidgetState::blur() noexcept {
    _focused.reset();
    _dragging = false;
}

void WidgetState::request_tab(bool backward) noexcept { _tab_backward = backward; }

TextEdit& WidgetState::draft() noexcept { return _draft; }

const std::string& WidgetState::original() const noexcept { return _original; }

bool WidgetState::dragging() const noexcept { return _dragging; }

void WidgetState::set_dragging(bool dragging) noexcept { _dragging = dragging; }

void WidgetState::add_to_tab_order(Clay_ElementId id) { _tab_order.push_back(id.id); }

bool WidgetState::is_open(Clay_ElementId id) const noexcept { return _popup == id.id; }

void WidgetState::open(Clay_ElementId id, std::size_t highlighted) noexcept {
    _popup = id.id;
    _highlighted = highlighted;
}

void WidgetState::cancel_tab() noexcept { _tab_backward.reset(); }

void WidgetState::close() noexcept { _popup.reset(); }

std::size_t& WidgetState::highlighted() noexcept { return _highlighted; }

void WidgetState::end_frame() {
    _pending = std::exchange(_requested, std::nullopt);
    if (_focused && !drawn(*_focused)) {
        blur();
    }
    resolve_tab();
    _tab_order.clear();
}

void WidgetState::resolve_tab() {
    const std::optional<bool> backward = std::exchange(_tab_backward, std::nullopt);
    const auto current = std::ranges::find(_tab_order, _focused.value_or(0));
    if (!backward || current == _tab_order.end()) {
        return;
    }
    const auto size = static_cast<std::ptrdiff_t>(_tab_order.size());
    const std::ptrdiff_t step = *backward ? size - 1 : 1;
    const std::ptrdiff_t next = (std::distance(_tab_order.begin(), current) + step) % size;
    blur();
    _pending = FocusRequest{.id = _tab_order[static_cast<std::size_t>(next)], .click_x = {}};
}

bool WidgetState::drawn(std::uint32_t id) const { return std::ranges::find(_tab_order, id) != _tab_order.end(); }

PaletteState& WidgetState::palette() noexcept { return _palette; }

SmoothScroll& WidgetState::scroll() noexcept { return _scroll; }

ViewportState& WidgetState::viewport() noexcept { return _viewport; }

} // namespace editor
