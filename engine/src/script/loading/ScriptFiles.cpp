#include "ScriptFiles.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace engine::script {

namespace {

namespace fs = std::filesystem;

bool is_entry_point(const fs::path& relative) { return relative == fs::path(entry_point); }

bool loads_before(const fs::path& left, const fs::path& right) {
    if (is_entry_point(left) != is_entry_point(right)) {
        return is_entry_point(right);
    }
    return left.generic_string() < right.generic_string();
}

} // namespace

std::vector<fs::path> collect_script_files(const fs::path& root, std::error_code& error) {
    std::vector<fs::path> files;
    const fs::path library = root / library_folder;
    fs::recursive_directory_iterator entries(root, fs::directory_options::skip_permission_denied,
                                             error);
    for (; !error && entries != fs::recursive_directory_iterator(); entries.increment(error)) {
        const fs::directory_entry& entry = *entries;
        if (entry.path() == library) {
            entries.disable_recursion_pending();
        } else if (entry.path().extension() == ".lua" && entry.is_regular_file(error)) {
            files.push_back(entry.path().lexically_relative(root));
        }
    }
    std::ranges::sort(files, loads_before);
    return files;
}

std::optional<std::string> read_text_file(const fs::path& path) {
    std::error_code error;
    if (!fs::is_regular_file(path, error)) {
        return std::nullopt;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return std::nullopt;
    }
    std::ostringstream content;
    content << stream.rdbuf();
    return std::move(content).str();
}

} // namespace engine::script
