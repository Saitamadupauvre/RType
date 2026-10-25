#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rtype::server {

inline constexpr std::size_t max_supported_players = 4;

struct ServerConfig {
    std::uint16_t port;
    std::size_t max_players = max_supported_players;

    bool operator==(const ServerConfig&) const = default;
};

struct CliResult {
    std::optional<ServerConfig> config;
    int exit_code;
    std::string message;
};

CliResult parse_cli(const std::vector<std::string>& args);

} // namespace rtype::server
