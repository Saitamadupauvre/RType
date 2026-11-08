#include "text/ScriptLocation.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>

namespace editor {

namespace {

constexpr std::string_view extension = ".lua:";
constexpr int max_line = 1'000'000;
constexpr int decimal_base = 10;

bool path_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '-' || c == '.' ||
           c == '/';
}

bool safe(const std::filesystem::path& path) {
    return !path.empty() && path.is_relative() &&
           std::ranges::none_of(path,
                                [](const std::filesystem::path& part) { return part == ".."; });
}

std::optional<int> line_number(std::string_view text) {
    const auto digits = static_cast<std::size_t>(
        std::ranges::find_if_not(
            text, [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }) -
        text.begin());
    if (digits == 0 || digits >= text.size() || text[digits] != ':') {
        return std::nullopt;
    }
    int line = 0;
    for (const char digit : text.substr(0, digits)) {
        line = (line * decimal_base) + (digit - '0');
        if (line > max_line) {
            return std::nullopt;
        }
    }
    if (line == 0) {
        return std::nullopt;
    }
    return line;
}

std::optional<ScriptLocation> location_at(std::string_view text, std::size_t found) {
    std::size_t start = found;
    while (start > 0 && path_char(text[start - 1])) {
        --start;
    }
    const std::size_t end = found + extension.size() - 1;
    const std::filesystem::path file(std::string(text.substr(start, end - start)));
    const auto line = line_number(text.substr(end + 1));
    if (start == found || !line || !safe(file)) {
        return std::nullopt;
    }
    return ScriptLocation{.file = file, .line = *line};
}

} // namespace

std::optional<ScriptLocation> script_location(std::string_view text) {
    for (std::size_t found = text.find(extension); found != std::string_view::npos;
         found = text.find(extension, found + 1)) {
        if (auto location = location_at(text, found)) {
            return location;
        }
    }
    return std::nullopt;
}

} // namespace editor
