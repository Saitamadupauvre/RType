#include "ui/Ui.hpp"

#include <utility>

#include "ui/Text.hpp"

namespace editor {

Ui::Ui(Theme theme, UiInput input) : _theme(theme), _input(std::move(input)) {}

const Theme& Ui::theme() const noexcept { return _theme; }

const UiInput& Ui::input() const noexcept { return _input; }

bool Ui::hovered(Clay_ElementId id) { return Clay_PointerOver(id); }

bool Ui::clicked(Clay_ElementId id) const { return _input.primary_pressed && hovered(id); }

void Ui::emit(Action action) { _actions.push_back(std::move(action)); }

std::vector<Action> Ui::take_actions() { return std::exchange(_actions, {}); }

Clay_String Ui::keep(std::string text) { return view_of(_texts.emplace_back(std::move(text))); }

} // namespace editor
