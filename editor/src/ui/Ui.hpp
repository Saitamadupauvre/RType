#pragma once

#include <clay.h>

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "action/Action.hpp"
#include "ui/Platform.hpp"
#include "ui/Theme.hpp"
#include "ui/UiInput.hpp"
#include "ui/WidgetState.hpp"

namespace editor {

/** @brief The mouse cursor a frame asks for. */
enum class Cursor : std::uint8_t { standard, resize_vertical };

/**
 * @brief Context of one UI frame: theme, input, emitted actions and the text they show.
 *
 * Must outlive the render of the frame it built: Clay keeps pointers into keep()'s strings.
 * The widget state and the platform are borrowed and must outlive it.
 */
class Ui {
public:
    Ui(Theme theme, UiInput input, WidgetState& widgets, const Platform& platform);

    [[nodiscard]] const Theme& theme() const noexcept;
    [[nodiscard]] const UiInput& input() const noexcept;
    [[nodiscard]] WidgetState& widgets() noexcept;
    [[nodiscard]] const WidgetState& widgets() const noexcept;
    [[nodiscard]] const Platform& platform() const noexcept;

    /** @brief Whether the pointer is over the element, as laid out in the previous frame. */
    [[nodiscard]] static bool hovered(Clay_ElementId id);
    /** @brief Whether the primary button went down over the element this frame. */
    [[nodiscard]] bool clicked(Clay_ElementId id) const;

    void emit(Action action);
    [[nodiscard]] std::vector<Action> take_actions();

    /** @brief Asks the platform to show @p cursor this frame; the last request wins. */
    void request_cursor(Cursor cursor) noexcept;
    /** @return The cursor requested this frame, Cursor::standard by default. */
    [[nodiscard]] Cursor cursor() const noexcept;

    /** @brief Stores a formatted string until the Ui is destroyed and returns a view of it. */
    [[nodiscard]] Clay_String keep(std::string text);

private:
    Theme _theme;
    UiInput _input;
    WidgetState* _widgets;
    const Platform* _platform;
    std::vector<Action> _actions;
    std::deque<std::string> _texts;
    Cursor _cursor = Cursor::standard;
};

} // namespace editor
