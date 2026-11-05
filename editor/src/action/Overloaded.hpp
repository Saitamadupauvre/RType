#pragma once

namespace editor {

/** @brief Visitor built from one lambda per alternative, for std::visit. */
template <typename... Handlers> struct Overloaded : Handlers... {
    using Handlers::operator()...;
};

} // namespace editor
