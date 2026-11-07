#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace editor {

/**
 * @brief Scores how well @p query matches @p label, ignoring case.
 *
 * Every character of the query must appear in the label, in order. Matches at the start of a
 * word and runs of consecutive matches score higher; gaps between matches score lower.
 * @return std::nullopt when the label does not contain the query; 0 for an empty query.
 */
[[nodiscard]] std::optional<int> fuzzy_score(std::string_view query, std::string_view label);

/**
 * @brief The indices of the labels matching @p query, best first.
 *
 * Labels with equal scores keep their order; an empty query keeps every label in order.
 */
[[nodiscard]] std::vector<std::size_t> fuzzy_rank(std::string_view query,
                                                  std::span<const std::string_view> labels);

} // namespace editor
