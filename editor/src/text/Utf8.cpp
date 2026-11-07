#include "text/Utf8.hpp"

#include <algorithm>
#include <cctype>

namespace editor {

namespace {

bool is_continuation(char byte) { return (static_cast<unsigned char>(byte) & 0xC0U) == 0x80U; }

bool is_word(char byte) {
    const auto value = static_cast<unsigned char>(byte);
    return value >= 0x80U || std::isalnum(value) != 0 || byte == '_';
}

} // namespace

std::size_t next_boundary(std::string_view text, std::size_t index) {
    if (index >= text.size()) {
        return text.size();
    }
    ++index;
    while (index < text.size() && is_continuation(text[index])) {
        ++index;
    }
    return index;
}

std::size_t previous_boundary(std::string_view text, std::size_t index) {
    index = std::min(index, text.size());
    if (index == 0) {
        return 0;
    }
    --index;
    while (index > 0 && is_continuation(text[index])) {
        --index;
    }
    return index;
}

std::size_t floor_boundary(std::string_view text, std::size_t index) {
    index = std::min(index, text.size());
    while (index > 0 && index < text.size() && is_continuation(text[index])) {
        --index;
    }
    return index;
}

std::size_t next_word_end(std::string_view text, std::size_t index) {
    index = std::min(index, text.size());
    while (index < text.size() && !is_word(text[index])) {
        ++index;
    }
    while (index < text.size() && is_word(text[index])) {
        ++index;
    }
    return index;
}

std::size_t previous_word_start(std::string_view text, std::size_t index) {
    index = std::min(index, text.size());
    while (index > 0 && !is_word(text[index - 1])) {
        --index;
    }
    while (index > 0 && is_word(text[index - 1])) {
        --index;
    }
    return index;
}

std::size_t word_start_at(std::string_view text, std::size_t index) {
    index = std::min(index, text.size());
    while (index > 0 && is_word(text[index - 1])) {
        --index;
    }
    return index;
}

std::size_t word_end_at(std::string_view text, std::size_t index) {
    index = std::min(index, text.size());
    while (index < text.size() && is_word(text[index])) {
        ++index;
    }
    return index;
}

} // namespace editor
