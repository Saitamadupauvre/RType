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
    Theme theme_;
    UiInput input_;
    std::vector<Action> actions_;
    std::deque<std::string> texts_;
};

} // namespace editor
