#include "Cli.hpp"
#include <iostream>

int main(int argc, char** argv) {
    const auto result = rtype::client::parse_cli({argv, argv + argc});

    if (!result.config.has_value()) {
        (result.exit_code == 0 ? std::cout : std::cerr) << result.message;
        return result.exit_code;
    }

    return 0;
}
