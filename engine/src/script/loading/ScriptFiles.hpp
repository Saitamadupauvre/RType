#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace engine::script {

inline constexpr std::string_view entry_point = "main.lua";
inline constexpr std::string_view library_folder = "lib";

std::vector<std::filesystem::path> collect_script_files(const std::filesystem::path& root,
                                                        std::error_code& error);

std::optional<std::string> read_text_file(const std::filesystem::path& path);

} // namespace engine::script
