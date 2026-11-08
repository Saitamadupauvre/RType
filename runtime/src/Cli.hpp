#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "engine/app/Launch.hpp"

namespace runtime {

inline constexpr std::size_t max_supported_players = 4;

struct RuntimeConfig {
    engine::app::Mode mode{engine::app::Mode::client};
    std::string server_ip{};
    std::uint16_t port{0};
    std::size_t max_players{max_supported_players};
    unsigned int music_volume{100};
    unsigned int effects_volume{100};
    bool muted{false};
    std::string game{};
    std::string scene_file{};

    bool operator==(const RuntimeConfig&) const = default;
};

struct CliResult {
    std::optional<RuntimeConfig> config;
    int exit_code;
    std::string message;
};

CliResult parse_cli(const std::vector<std::string>& args);

} // namespace runtime
