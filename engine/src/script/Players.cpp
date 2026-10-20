#include "Players.hpp"

#include <algorithm>
#include <utility>

namespace engine::script {

bool Players::join(std::uint32_t id) {
    if (_players.contains(id)) {
        return false;
    }
    _players.emplace(id, Player{.slot = free_slot(), .input = {}});
    return true;
}

bool Players::leave(std::uint32_t id) { return _players.erase(id) > 0; }

void Players::set_input(std::uint32_t id, InputState input) {
    const auto found = _players.find(id);
    if (found != _players.end()) {
        found->second.input = std::move(input);
    }
}

bool Players::contains(std::uint32_t id) const { return _players.contains(id); }

std::optional<std::size_t> Players::slot(std::uint32_t id) const {
    const auto found = _players.find(id);
    if (found == _players.end()) {
        return std::nullopt;
    }
    return found->second.slot;
}

const InputState* Players::input(std::uint32_t id) const {
    const auto found = _players.find(id);
    return found == _players.end() ? nullptr : &found->second.input;
}

std::vector<std::uint32_t> Players::ids() const {
    std::vector<std::uint32_t> ids;
    ids.reserve(_players.size());
    for (const auto& [id, player] : _players) {
        ids.push_back(id);
    }
    return ids;
}

std::size_t Players::free_slot() const {
    std::size_t slot = 0;
    while (std::ranges::any_of(_players,
                               [slot](const auto& entry) { return entry.second.slot == slot; })) {
        ++slot;
    }
    return slot;
}

} // namespace engine::script
