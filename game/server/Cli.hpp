#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rtype::server {

struct ServerConfig {
    std::uint16_t port;

    bool operator==(const ServerConfig&) const = default;
};

struct CliResult {
    std::optional<ServerConfig> config;
    int exit_code;
    std::string message;
};

CliResult parse_cli(const std::vector<std::string>& args);

} // namespace rtype::server
