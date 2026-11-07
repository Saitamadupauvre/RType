#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "engine/core/log/Level.hpp"

namespace editor {

struct EditorConfig {
    std::filesystem::path game;
    /** @brief Program that plays the game; empty for the default next to the editor. */
    std::filesystem::path runtime{};
    /** @brief Lowest level the developer log writes to stderr. */
    engine::core::log::Level log_level = engine::core::log::Level::Info;

    bool operator==(const EditorConfig&) const = default;
};

/** @brief A config to run with, or a message to print and the exit code to return. */
struct CliResult {
    std::optional<EditorConfig> config;
    int exit_code;
    std::string message;
};

/** @brief Parses `args`, program name first. Help exits with 0, any error with 84. */
CliResult parse_cli(const std::vector<std::string>& args);

} // namespace editor
