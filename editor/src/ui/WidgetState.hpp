#pragma once

#include <clay.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "text/TextEdit.hpp"
#include "ui/SmoothScroll.hpp"
#include "viewport/ViewportState.hpp"

namespace editor {

/** @brief A field asked to take focus, by a click at `click_x` or, without it, by Tab. */
struct FocusRequest {
    std::uint32_t id = 0;
    std::optional<float> click_x;
};

/** @brief The command palette: whether it is open, what was typed and the highlighted row. */
struct PaletteState {
    bool open = false;
    std::string query;
    std::size_t highlighted = 0;
};

/**
 * @brief The only widget state kept between frames: focus, the focused field's draft, the
 * open popup, the scroll targets and the viewport camera.
 *
 * Owned by the frame loop and lent to each frame's Ui. Focus changes requested during a frame
 * apply on the next one, so the field losing focus always gets to commit first.
 */
class WidgetState {
public:
    /** @brief Whether a field is focused; the viewport and the shortcuts ignore keys then. */
    [[nodiscard]] bool wants_keyboard() const noexcept;

    /** @brief Whether the field is focused. */
    [[nodiscard]] bool is_focused(Clay_ElementId id) const noexcept;
    /** @brief The request for the field to take focus on this frame, if there is one. */
    [[nodiscard]] std::optional<FocusRequest> focus_requested(Clay_ElementId id) const noexcept;
    /** @brief Asks for the field clicked at `click_x` to take focus on the next frame. */
    void request_focus(Clay_ElementId id, float click_x);
    /** @brief Focuses the field now and starts editing `value`, kept to be restored on Esc. */
    void focus(Clay_ElementId id, std::string_view value);
    void blur() noexcept;
    /** @brief Moves focus to the next (or previous) field drawn this frame, at the end of it. */
    void request_tab(bool backward) noexcept;
    /** @brief Drops the Tab move requested this frame, keeping focus where it is. */
    void cancel_tab() noexcept;

    /** @pre A field is focused. */
    [[nodiscard]] TextEdit& draft() noexcept;
    /** @pre A field is focused. */
    [[nodiscard]] const std::string& original() const noexcept;

    /** @brief How far the focused field's text is scrolled left, in pixels; 0 on focus. */
    [[nodiscard]] float& text_offset() noexcept;

    /** @brief Whether the focused field is selecting with the primary button held. */
    [[nodiscard]] bool dragging() const noexcept;
    void set_dragging(bool dragging) noexcept;

    /** @brief Records a focusable field drawn this frame, in Tab order. */
    void add_to_tab_order(Clay_ElementId id);

    [[nodiscard]] bool is_open(Clay_ElementId id) const noexcept;
    /** @brief Opens the popup with `highlighted` as its keyboard choice, closing any other. */
    void open(Clay_ElementId id, std::size_t highlighted) noexcept;
    void close() noexcept;
    /** @pre A popup is open. */
    [[nodiscard]] std::size_t& highlighted() noexcept;

    [[nodiscard]] PaletteState& palette() noexcept;
    [[nodiscard]] SmoothScroll& scroll() noexcept;
    [[nodiscard]] ViewportState& viewport() noexcept;

    /** @brief Applies Tab moves, drops focus from fields not drawn and starts the next frame. */
    void end_frame();

private:
    void resolve_tab();
    [[nodiscard]] bool drawn(std::uint32_t id) const;

    std::optional<std::uint32_t> _focused;
    TextEdit _draft;
    std::string _original;
    std::optional<FocusRequest> _requested;
    std::optional<FocusRequest> _pending;
    std::optional<bool> _tab_backward;
    float _text_offset = 0.F;
    bool _dragging = false;
    std::vector<std::uint32_t> _tab_order;
    std::optional<std::uint32_t> _popup;
    std::size_t _highlighted = 0;
    PaletteState _palette;
    SmoothScroll _scroll;
    ViewportState _viewport;
};

} // namespace editor
