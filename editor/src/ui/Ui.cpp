#include "ui/Ui.hpp"

#include <utility>

#include "ui/Text.hpp"

namespace editor {

Ui::Ui(Theme theme, UiInput input) : theme_(theme), input_(input) {}

const Theme& Ui::theme() const noexcept { return theme_; }

const UiInput& Ui::input() const noexcept { return input_; }

bool Ui::hovered(Clay_ElementId id) { return Clay_PointerOver(id); }

bool Ui::clicked(Clay_ElementId id) const { return input_.primary_pressed && hovered(id); }

void Ui::emit(Action action) { actions_.push_back(std::move(action)); }

std::vector<Action> Ui::take_actions() { return std::exchange(actions_, {}); }

Clay_String Ui::keep(std::string text) { return view_of(texts_.emplace_back(std::move(text))); }

} // namespace editor
