#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rtype::client {

struct ClientConfig {
    std::string host;
    std::uint16_t port;
};

struct CliResult {
    std::optional<ClientConfig> config;
    int exit_code;
    std::string message;
};

CliResult parse_cli(const std::vector<std::string>& args);

} // namespace rtype::client
