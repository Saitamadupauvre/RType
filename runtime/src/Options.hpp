#pragma once

#include <filesystem>
#include <string>

#include "Cli.hpp"
#include "engine/app/Launch.hpp"

namespace runtime {

struct PluginFiles {
    std::string graphics;
    std::string audio;
};

std::string window_title(const std::filesystem::path& game);

engine::app::LaunchOptions launch_options(const RuntimeConfig& config,
                                          const std::filesystem::path& executable_dir,
                                          const PluginFiles& plugins);

} // namespace runtime
