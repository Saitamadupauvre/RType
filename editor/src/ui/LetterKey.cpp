#include "ui/LetterKey.hpp"

#include <array>
#include <cctype>
#include <utility>

namespace editor {

namespace {

constexpr std::array letters{
    std::pair{'a', Key::a},    std::pair{'c', Key::c},     std::pair{'s', Key::s},
    std::pair{'v', Key::v},    std::pair{'x', Key::x},     std::pair{'y', Key::y},
    std::pair{'z', Key::z},    std::pair{'g', Key::g},     std::pair{'h', Key::h},
    std::pair{'p', Key::p},    std::pair{'d', Key::d},     std::pair{'=', Key::plus},
    std::pair{'+', Key::plus}, std::pair{'-', Key::minus},
};

} // namespace

std::optional<Key> letter_key(std::string_view name) {
    if (name.size() != 1) {
        return std::nullopt;
    }
    const auto letter = static_cast<char>(std::tolower(static_cast<unsigned char>(name.front())));
    for (const auto& [candidate, key] : letters) {
        if (candidate == letter) {
            return key;
        }
    }
    return std::nullopt;
}

} // namespace editor
