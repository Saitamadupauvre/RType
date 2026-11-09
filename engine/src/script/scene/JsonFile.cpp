#include "JsonFile.hpp"

#include <format>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>

#include "engine/core/log/Log.hpp"

namespace engine::script {

namespace {

namespace fs = std::filesystem;
namespace log = engine::core::log;

constexpr int indent = 4;

bool create_parent(const fs::path& file) {
    if (!file.has_parent_path()) {
        return true;
    }
    std::error_code error;
    fs::create_directories(file.parent_path(), error);
    if (error) {
        log::error("json: cannot create {}: {}", file.parent_path().generic_string(),
                   error.message());
        return false;
    }
    return true;
}

bool write_text(const fs::path& file, const std::string& text) {
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream << text;
    stream.close();
    if (!stream) {
        log::error("json: cannot write {}", file.generic_string());
        return false;
    }
    return true;
}

bool replace(const fs::path& temporary, const fs::path& file) {
    std::error_code error;
    fs::rename(temporary, file, error);
    if (error) {
        log::error("json: cannot replace {}: {}", file.generic_string(), error.message());
        return false;
    }
    return true;
}

} // namespace

Json read_json_file(const fs::path& file) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        throw std::runtime_error(std::format("cannot open {}", file.generic_string()));
    }
    const std::string text{std::istreambuf_iterator<char>(stream), {}};
    try {
        return Json::parse(text);
    } catch (const Json::exception& error) {
        throw std::runtime_error(
            std::format("{} is not valid JSON: {}", file.generic_string(), error.what()));
    }
}

bool write_json_file(const fs::path& file, const Json& content) {
    fs::path temporary = file;
    temporary += ".tmp";
    if (create_parent(file) && write_text(temporary, content.dump(indent) + "\n") &&
        replace(temporary, file)) {
        return true;
    }
    std::error_code ignored;
    fs::remove(temporary, ignored);
    return false;
}

} // namespace engine::script
