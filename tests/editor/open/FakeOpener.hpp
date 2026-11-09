#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "open/FileOpener.hpp"

struct FakeOpenings {
    std::vector<std::filesystem::path> opened;
    std::vector<std::string> failures;
};

class FakeOpener final : public editor::FileOpener {
public:
    explicit FakeOpener(std::shared_ptr<FakeOpenings> openings) : _openings(std::move(openings)) {}

    bool open(const std::filesystem::path& file) override {
        _openings->opened.push_back(file);
        return file.filename() != "locked.lua";
    }

    std::vector<std::string> take_failures() override {
        return std::exchange(_openings->failures, {});
    }

private:
    std::shared_ptr<FakeOpenings> _openings;
};
