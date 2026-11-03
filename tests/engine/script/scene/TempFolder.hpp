#pragma once

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>

class TempFolder {
public:
    TempFolder() : _path(std::filesystem::path(::testing::TempDir()) / unique_name()) {
        std::filesystem::remove_all(_path);
        std::filesystem::create_directories(_path);
    }

    ~TempFolder() {
        std::error_code ignored;
        std::filesystem::remove_all(_path, ignored);
    }

    TempFolder(const TempFolder&) = delete;
    TempFolder& operator=(const TempFolder&) = delete;
    TempFolder(TempFolder&&) = delete;
    TempFolder& operator=(TempFolder&&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return _path; }

    void write(const std::filesystem::path& relative, std::string_view text) const {
        const std::filesystem::path file = _path / relative;
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary) << text;
    }

    [[nodiscard]] std::string read(const std::filesystem::path& relative) const {
        std::ifstream stream(_path / relative, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream), {}};
    }

private:
    static std::string unique_name() {
        const auto* test = ::testing::UnitTest::GetInstance()->current_test_info();
        return std::string("rtype_") + test->test_suite_name() + "_" + test->name();
    }

    std::filesystem::path _path;
};
