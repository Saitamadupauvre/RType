#pragma once

#include <clay.h>

#include <deque>
#include <string>
#include <vector>

#include "action/Action.hpp"
#include "ui/Theme.hpp"
#include "ui/UiInput.hpp"

namespace editor {

class Ui {
public:
    Ui(Theme theme, UiInput input);

    [[nodiscard]] const Theme& theme() const noexcept;
    [[nodiscard]] const UiInput& input() const noexcept;

    [[nodiscard]] static bool hovered(Clay_ElementId id);
    [[nodiscard]] bool clicked(Clay_ElementId id) const;

    void emit(Action action);
    [[nodiscard]] std::vector<Action> take_actions();

    [[nodiscard]] Clay_String keep(std::string text);

private:
    Theme _theme;
    UiInput _input;
    std::vector<Action> _actions;
    std::deque<std::string> _texts;
};

} // namespace editor
