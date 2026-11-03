#pragma once

#include <filesystem>

#include "JsonFieldValue.hpp"

namespace engine::script {

Json read_json_file(const std::filesystem::path& file);

bool write_json_file(const std::filesystem::path& file, const Json& content);

} // namespace engine::script
