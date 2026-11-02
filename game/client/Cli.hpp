#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "engine/app/Launch.hpp"

namespace rtype::client {

inline constexpr std::size_t max_players = 4;

struct ClientConfig {
    engine::app::Mode mode{engine::app::Mode::client};
    std::string host;
    std::uint16_t port{0};
    unsigned int music_volume{100};
    unsigned int effects_volume{100};
    bool muted{false};

    bool operator==(const ClientConfig&) const = default;
};

struct CliResult {
    std::optional<ClientConfig> config;
    int exit_code;
    std::string message;
};

CliResult parse_cli(const std::vector<std::string>& args);

} // namespace rtype::client
