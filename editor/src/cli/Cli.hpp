#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace editor {

struct EditorConfig {
    std::filesystem::path game;

    bool operator==(const EditorConfig&) const = default;
};

struct CliResult {
    std::optional<EditorConfig> config;
    int exit_code;
    std::string message;
};

CliResult parse_cli(const std::vector<std::string>& args);

} // namespace editor
