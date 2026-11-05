#pragma once

namespace editor {

template <typename... Handlers> struct Overloaded : Handlers... {
    using Handlers::operator()...;
};

} // namespace editor
