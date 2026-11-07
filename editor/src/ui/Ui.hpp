#pragma once

#include <clay.h>

#include <deque>
#include <string>
#include <vector>

#include "action/Action.hpp"
#include "ui/Theme.hpp"
#include "ui/UiInput.hpp"

namespace editor {

/**
 * @brief Context of one UI frame: theme, input, emitted actions and the text they show.
 *
 * Must outlive the render of the frame it built: Clay keeps pointers into keep()'s strings.
 */
class Ui {
public:
    Ui(Theme theme, UiInput input);

    [[nodiscard]] const Theme& theme() const noexcept;
    [[nodiscard]] const UiInput& input() const noexcept;

    /** @brief Whether the pointer is over the element, as laid out in the previous frame. */
    [[nodiscard]] static bool hovered(Clay_ElementId id);
    /** @brief Whether the primary button went down over the element this frame. */
    [[nodiscard]] bool clicked(Clay_ElementId id) const;

    void emit(Action action);
    [[nodiscard]] std::vector<Action> take_actions();

    /** @brief Stores a formatted string until the Ui is destroyed and returns a view of it. */
    [[nodiscard]] Clay_String keep(std::string text);

private:
    Theme _theme;
    UiInput _input;
    std::vector<Action> _actions;
    std::deque<std::string> _texts;
};

} // namespace editor
