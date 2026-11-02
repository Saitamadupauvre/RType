#include "engine/script/ServerApi.hpp"

#include <exception>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "RuntimeContext.hpp"
#include "engine/core/log/Log.hpp"
#include "input/LuaInput.hpp"
#include "lua/LuaErrors.hpp"
#include "player/Players.hpp"

namespace engine::script {

namespace {

namespace log = engine::core::log;

constexpr std::string_view on_player_joined = "on_player_joined";
constexpr std::string_view on_player_left = "on_player_left";

const core::InputState neutral_input{};

bool is_nil(const sol::object& value) {
    return value.get_type() == sol::type::lua_nil || value.get_type() == sol::type::none;
}

} // namespace

struct ServerApi::State {
    RuntimeContext* context = nullptr;
    Players players;
    std::unordered_map<core::Entity::Id, std::uint32_t> owners;
    std::unordered_map<core::Entity::Id, core::InputState> inputs;

    void install(RuntimeContext& runtime);
    void bind_players();
    void bind_entity_fields();
    void bind_globals();
    sol::object spawn(sol::this_state state, const std::string& type,
                      const sol::optional<sol::table>& overrides) const;
    [[nodiscard]] sol::object player_object(sol::this_state state, std::uint32_t player_id) const;
    sol::object player_field(sol::this_state state, const PlayerHandle& player,
                             const std::string& key) const;
    sol::table player_entities(sol::this_state state, const PlayerHandle& player) const;
    sol::table all_players(sol::this_state state) const;
    [[nodiscard]] std::vector<core::Entity> owned_by(std::uint32_t player_id) const;
    [[nodiscard]] std::optional<std::uint32_t> owner_of(core::Entity entity) const;
    [[nodiscard]] const core::InputState& input_of(core::Entity entity) const;
    void set_owner(sol::this_state state, core::Entity entity, const sol::object& value);
    bool joined(std::uint32_t player_id);
    bool left(std::uint32_t player_id);
};

void ServerApi::State::install(RuntimeContext& runtime) {
    context = &runtime;
    bind_players();
    bind_entity_fields();
    bind_globals();
    runtime.entities.on_release([this](core::Entity entity) {
        owners.erase(entity.id());
        inputs.erase(entity.id());
    });
}

void ServerApi::State::bind_players() {
    context->types.new_usertype<PlayerHandle>(
        "PlayerHandle", sol::no_constructor, sol::meta_function::index,
        [this](sol::this_state state, const PlayerHandle& player, const std::string& key) {
            return player_field(state, player, key);
        },
        sol::meta_function::equal_to,
        [](const PlayerHandle& left, const PlayerHandle& right) { return left.id == right.id; });
    bind_input_state(context->types);
}

void ServerApi::State::bind_entity_fields() {
    context->entities.add_handle_field(
        "input", HandleField{.read =
                                 [this](sol::this_state state, core::Entity entity) {
                                     return sol::make_object(state, input_of(entity));
                                 },
                             .write = {}});
    context->entities.add_handle_field(
        "owner",
        HandleField{
            .read =
                [this](sol::this_state state, core::Entity entity) {
                    const auto owner = owner_of(entity);
                    return owner ? player_object(state, *owner)
                                 : sol::make_object(state, sol::lua_nil);
                },
            .write = [this](sol::this_state state, core::Entity entity,
                            const sol::object& value) { set_owner(state, entity, value); }});
}

void ServerApi::State::bind_globals() {
    sol::state& lua = context->lua;
    lua.set_function("spawn", [this](sol::this_state state, const std::string& type,
                                     const sol::optional<sol::table>& overrides) {
        return spawn(state, type, overrides);
    });
    lua.set_function("players", [this](sol::this_state state) { return all_players(state); });
    lua.set_function("find_player", [this](sol::this_state state, lua_Integer id) {
        return player_object(state, static_cast<std::uint32_t>(id));
    });
    lua.set_function(
        "play", [this](sol::this_state state, const sol::object& name, const sol::object& source) {
            context->audio.play(state, name, source);
        });
    lua.set_function("music", [this](sol::this_state state, const sol::object& name) {
        context->audio.music(state, name);
    });
    lua.set_function("emit", [this](sol::this_state state, const sol::object& name,
                                    const sol::object& arguments) {
        context->events.emit(state, name, arguments);
    });
}

sol::object ServerApi::State::spawn(sol::this_state state, const std::string& type,
                                    const sol::optional<sol::table>& overrides) const {
    const Declaration* declaration = context->find_declaration(type);
    if (declaration == nullptr) {
        log::error("lua: {}cannot spawn unknown entity '{}'", caller_location(state), type);
        return sol::make_object(state, sol::lua_nil);
    }
    return context->entities.spawn_from_lua(state, *declaration, overrides);
}

sol::object ServerApi::State::player_object(sol::this_state state, std::uint32_t player_id) const {
    if (!players.contains(player_id)) {
        return sol::make_object(state, sol::lua_nil);
    }
    return sol::make_object(state, PlayerHandle{player_id});
}

sol::object ServerApi::State::player_field(sol::this_state state, const PlayerHandle& player,
                                           const std::string& key) const {
    if (key == "id") {
        return sol::make_object(state, static_cast<lua_Integer>(player.id));
    }
    if (key == "slot") {
        const auto slot = players.slot(player.id);
        return slot ? sol::make_object(state, static_cast<lua_Integer>(*slot))
                    : sol::make_object(state, sol::lua_nil);
    }
    if (key == "connected") {
        return sol::make_object(state, players.contains(player.id));
    }
    if (key == "entities") {
        return sol::make_object(state, [this](sol::this_state inner, const PlayerHandle& self) {
            return player_entities(inner, self);
        });
    }
    return sol::make_object(state, sol::lua_nil);
}

sol::table ServerApi::State::player_entities(sol::this_state state,
                                             const PlayerHandle& player) const {
    sol::table owned = sol::state_view(state).create_table();
    for (const core::Entity entity : owned_by(player.id)) {
        owned.add(context->entities.handle_of(entity));
    }
    return owned;
}

sol::table ServerApi::State::all_players(sol::this_state state) const {
    sol::table result = sol::state_view(state).create_table();
    for (const std::uint32_t id : players.ids()) {
        result.add(PlayerHandle{id});
    }
    return result;
}

std::vector<core::Entity> ServerApi::State::owned_by(std::uint32_t player_id) const {
    std::vector<core::Entity> owned;
    for (const core::Entity entity : context->entities.live_entities()) {
        const auto owner = owners.find(entity.id());
        if (owner != owners.end() && owner->second == player_id &&
            context->entities.is_active(entity)) {
            owned.push_back(entity);
        }
    }
    return owned;
}

std::optional<std::uint32_t> ServerApi::State::owner_of(core::Entity entity) const {
    const auto found = owners.find(entity.id());
    if (found == owners.end() || !players.contains(found->second)) {
        return std::nullopt;
    }
    return found->second;
}

const core::InputState& ServerApi::State::input_of(core::Entity entity) const {
    if (const auto owner = owner_of(entity)) {
        if (const core::InputState* owned = players.input(*owner)) {
            return *owned;
        }
    }
    const auto found = inputs.find(entity.id());
    return found == inputs.end() ? neutral_input : found->second;
}

void ServerApi::State::set_owner(sol::this_state state, core::Entity entity,
                                 const sol::object& value) {
    if (is_nil(value)) {
        owners.erase(entity.id());
        return;
    }
    if (!value.is<PlayerHandle>()) {
        throw std::runtime_error(
            std::format("{}entity field 'owner' must be a player or nil", caller_location(state)));
    }
    const std::uint32_t player_id = value.as<PlayerHandle>().id;
    if (!players.contains(player_id)) {
        throw std::runtime_error(
            std::format("{}player {} is not connected", caller_location(state), player_id));
    }
    owners.insert_or_assign(entity.id(), player_id);
}

bool ServerApi::State::joined(std::uint32_t player_id) {
    if (!players.join(player_id)) {
        log::warn("lua: player {} is already connected", player_id);
        return false;
    }
    const auto callback = context->main_callback(on_player_joined);
    if (!callback.valid()) {
        return true;
    }
    return report(callback(PlayerHandle{player_id}));
}

bool ServerApi::State::left(std::uint32_t player_id) {
    if (!players.contains(player_id)) {
        log::warn("lua: unknown player {} left", player_id);
        return false;
    }
    const auto callback = context->main_callback(on_player_left);
    bool success = true;
    if (callback.valid()) {
        success = report(callback(PlayerHandle{player_id}));
    } else {
        for (const core::Entity entity : owned_by(player_id)) {
            context->entities.doom(entity);
        }
    }
    players.leave(player_id);
    return success;
}

ServerApi::ServerApi() : _state(std::make_unique<State>()) {}

ServerApi::~ServerApi() = default;

std::string_view ServerApi::section() const noexcept { return "server"; }

void ServerApi::install(RuntimeContext& context) { _state->install(context); }

void ServerApi::set_input(core::Entity entity, core::InputState input) noexcept {
    if (_state->context->entities.type_of(entity)) {
        _state->inputs.insert_or_assign(entity.id(), std::move(input));
    }
}

bool ServerApi::player_joined(std::uint32_t player_id) noexcept {
    try {
        return _state->joined(player_id);
    } catch (const std::exception& exception) {
        log::error("lua: {}: {}", on_player_joined, exception.what());
    } catch (...) {
        log::error("lua: {}: unknown error", on_player_joined);
    }
    return false;
}

bool ServerApi::player_left(std::uint32_t player_id) noexcept {
    try {
        return _state->left(player_id);
    } catch (const std::exception& exception) {
        log::error("lua: {}: {}", on_player_left, exception.what());
    } catch (...) {
        log::error("lua: {}: unknown error", on_player_left);
    }
    return false;
}

void ServerApi::set_player_input(std::uint32_t player_id, core::InputState input) noexcept {
    _state->players.set_input(player_id, std::move(input));
}

std::optional<std::uint32_t> ServerApi::entity_owner(core::Entity entity) const {
    return _state->owner_of(entity);
}

std::vector<AudioCommand> ServerApi::take_audio_commands() noexcept {
    return _state->context->audio.take_commands();
}

std::vector<EventCommand> ServerApi::take_event_commands() noexcept {
    return _state->context->events.take_commands();
}

} // namespace engine::script
