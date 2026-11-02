#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "engine/core/FieldRegistry.hpp"
#include "engine/script/EventCommand.hpp"
#include "lua/Sol.hpp"

namespace engine::script {

inline constexpr std::size_t max_event_name_length = 63;
inline constexpr std::size_t max_queued_events = 64;

class Events {
public:
    void allow_declarations(bool allowed) noexcept { _declarations_allowed = allowed; }

    sol::object declare(sol::this_state state, const sol::object& name);
    void emit(sol::this_state state, const sol::object& name, const sol::object& arguments);

    [[nodiscard]] bool contains(std::string_view name) const;
    [[nodiscard]] std::vector<std::string> names() const;
    [[nodiscard]] std::vector<std::string> keys() const;
    [[nodiscard]] std::vector<EventCommand> take_commands() noexcept;

private:
    using Schema = std::map<std::string, core::FieldKind, std::less<>>;

    void define(sol::this_state state, const std::string& name, const sol::object& schema);
    void queue(EventCommand command);

    std::map<std::string, Schema, std::less<>> _schemas;
    std::vector<EventCommand> _commands;
    bool _declarations_allowed{false};
    bool _overflow_reported{false};
};

} // namespace engine::script
