#include "text/FuzzyMatch.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace editor {

namespace {

constexpr int match_score = 1;
constexpr int consecutive_bonus = 5;
constexpr int word_start_bonus = 8;

char lowered(char character) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
}

bool word_start(std::string_view label, std::size_t index) {
    if (index == 0) {
        return true;
    }
    const char before = label[index - 1];
    return before == ' ' || before == '-' || before == '_' || before == '.';
}

} // namespace

std::optional<int> fuzzy_score(std::string_view query, std::string_view label) {
    if (query.empty()) {
        return 0;
    }
    const auto at = [&label](std::size_t index) {
        return match_score + (word_start(label, index) ? word_start_bonus : 0);
    };
    std::vector<std::optional<int>> best(label.size());
    for (std::size_t index = 0; index < label.size(); ++index) {
        if (lowered(label[index]) == lowered(query.front())) {
            best[index] = at(index);
        }
    }
    for (const char wanted : query.substr(1)) {
        std::vector<std::optional<int>> next(label.size());
        for (std::size_t index = 0; index < label.size(); ++index) {
            if (lowered(label[index]) != lowered(wanted)) {
                continue;
            }
            for (std::size_t before = 0; before < index; ++before) {
                if (!best[before]) {
                    continue;
                }
                const int link = before + 1 == index ? consecutive_bonus
                                                     : -static_cast<int>(index - before - 1);
                const int score = best[before].value_or(0) + link + at(index);
                next[index] = std::max(next[index].value_or(score), score);
            }
        }
        best = std::move(next);
    }
    std::optional<int> result;
    for (const std::optional<int>& score : best) {
        if (score && (!result || *score > *result)) {
            result = score;
        }
    }
    return result;
}

std::vector<std::size_t> fuzzy_rank(std::string_view query,
                                    std::span<const std::string_view> labels) {
    std::vector<std::pair<int, std::size_t>> scored;
    for (std::size_t index = 0; index < labels.size(); ++index) {
        if (const std::optional<int> score = fuzzy_score(query, labels[index])) {
            scored.emplace_back(*score, index);
        }
    }
    std::ranges::stable_sort(scored, [](const auto& left, const auto& right) {
        return left.first > right.first;
    });
    std::vector<std::size_t> ranked;
    ranked.reserve(scored.size());
    for (const auto& [score, index] : scored) {
        ranked.push_back(index);
    }
    return ranked;
}

} // namespace editor
