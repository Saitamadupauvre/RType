#pragma once

#include <cstddef>
#include <string_view>

namespace editor {

[[nodiscard]] std::size_t next_boundary(std::string_view text, std::size_t index);
[[nodiscard]] std::size_t previous_boundary(std::string_view text, std::size_t index);
[[nodiscard]] std::size_t floor_boundary(std::string_view text, std::size_t index);

[[nodiscard]] std::size_t next_word_end(std::string_view text, std::size_t index);
[[nodiscard]] std::size_t previous_word_start(std::string_view text, std::size_t index);

[[nodiscard]] std::size_t word_start_at(std::string_view text, std::size_t index);
[[nodiscard]] std::size_t word_end_at(std::string_view text, std::size_t index);

} // namespace editor
