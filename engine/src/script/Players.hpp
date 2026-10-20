#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "engine/script/InputState.hpp"

namespace engine::script {

struct PlayerHandle {
    std::uint32_t id;
};

class Players {
public:
    bool join(std::uint32_t id);
    bool leave(std::uint32_t id);
    void set_input(std::uint32_t id, InputState input);

    [[nodiscard]] bool contains(std::uint32_t id) const;
    [[nodiscard]] std::optional<std::size_t> slot(std::uint32_t id) const;
    [[nodiscard]] const InputState* input(std::uint32_t id) const;
    [[nodiscard]] std::vector<std::uint32_t> ids() const;

private:
    struct Player {
        std::size_t slot;
        InputState input;
    };

    [[nodiscard]] std::size_t free_slot() const;

    std::map<std::uint32_t, Player> _players;
};

} // namespace engine::script
