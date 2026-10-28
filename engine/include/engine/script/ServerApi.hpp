#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "engine/core/Entity.hpp"
#include "engine/core/InputState.hpp"
#include "engine/script/ApiSet.hpp"
#include "engine/script/AudioCommand.hpp"
#include "engine/script/EventCommand.hpp"
#include "engine/script/Export.hpp"

namespace engine::script {

class ScriptRuntimeBuilder;

/**
 * @brief API set of the authoritative side: spawning, players, input and replicated requests.
 *
 * Adds the Lua globals `spawn`, `players`, `find_player`, `play`, `music` and `emit`, the entity
 * fields `self.input` and `self.owner`, the `main.lua` callbacks `on_player_joined` and
 * `on_player_left`, and reads the `server = { ... }` section of declarations.
 *
 * @note Not thread safe. Created by ScriptRuntimeBuilder::with(), reached with
 * ScriptRuntime::api(), destroyed with its runtime.
 */
class ENGINE_SCRIPT_EXPORT ServerApi final : public ApiSet {
public:
    ~ServerApi() override;

    ServerApi(const ServerApi&) = delete;
    ServerApi& operator=(const ServerApi&) = delete;
    ServerApi(ServerApi&&) = delete;
    ServerApi& operator=(ServerApi&&) = delete;

    /** @brief Returns `"server"`. */
    [[nodiscard]] std::string_view section() const noexcept override;

    /**
     * @brief Sets the input an entity reads through `self.input` until the next call.
     *
     * Entities never given an input read a neutral one: nothing is held and there is no pointer.
     *
     * @param entity Entity spawned by the runtime. Unknown entities are ignored.
     * @param input Input for the coming ticks.
     */
    void set_input(core::Entity entity, core::InputState input) noexcept;

    /**
     * @brief Registers a connected player and calls `on_player_joined(player)` from `main.lua`.
     *
     * The player gets the lowest free slot, starting at 0, and a neutral input. Scripts give it
     * entities by setting their `owner` field; those entities then read the player's input
     * through `self.input`. The callback is optional and protected: an error is logged.
     *
     * @param player_id Identifier chosen by the caller, unique among connected players.
     * @return true if the player was added and the callback, when defined, ran to completion;
     * false if @p player_id is already connected (logged, nothing changes) or the callback failed.
     * @note Never throws.
     */
    bool player_joined(std::uint32_t player_id) noexcept;

    /**
     * @brief Calls `on_player_left(player)` from `main.lua`, then forgets the player.
     *
     * Without that callback, every entity the player owns is marked for destruction and removed
     * by the next ScriptRuntime::flush_destroyed(). With it, the script decides; entities still
     * owned afterwards read a nil `owner` and a neutral input.
     *
     * @param player_id Player registered by player_joined().
     * @return true if the player was removed and the callback, when defined, ran to completion;
     * false if @p player_id is unknown (logged) or the callback failed. The player is removed
     * even when the callback fails.
     * @note Never throws.
     */
    bool player_left(std::uint32_t player_id) noexcept;

    /**
     * @brief Sets the input read by every entity owned by a player until the next call.
     *
     * @param player_id Player registered by player_joined(). Unknown players are ignored.
     * @param input Input for the coming ticks.
     */
    void set_player_input(std::uint32_t player_id, core::InputState input) noexcept;

    /**
     * @brief Returns the connected player that owns an entity.
     *
     * @return The owner's id, or std::nullopt if the entity is unknown, has no owner, or its
     * owner has left.
     */
    [[nodiscard]] std::optional<std::uint32_t> entity_owner(core::Entity entity) const;

    /**
     * @brief Returns the audio requests queued by scripts since the last call, in call order,
     * and empties the queue.
     *
     * Scripts queue them with `play(name)`, `play(name, entity)`, `music(name)` and
     * `music(nil)`, using names declared with `sounds { ... }` and `musics { ... }`. The runtime
     * plays nothing itself: the host plays the commands or sends them to clients, typically once
     * per tick. An undeclared name or an argument of the wrong type raises a Lua error, logged by
     * the protected call that runs the script. At most 64 commands are kept between two calls;
     * later ones are dropped and logged once.
     *
     * @note Never throws.
     */
    [[nodiscard]] std::vector<AudioCommand> take_audio_commands() noexcept;

    /**
     * @brief Returns the events queued by scripts since the last call, in call order, and
     * empties the queue.
     *
     * Scripts queue them with `emit(name)` or `emit(name, { key = value, ... })`, using events
     * declared with `event "name" { ... }`. Keys left out are absent from the command. An
     * undeclared event or key, a value of the wrong kind, a non-finite float or a string longer
     * than `max_event_text_length` bytes raises a Lua error and queues nothing. At most 64 events
     * are kept between two calls; later ones are dropped and logged once.
     *
     * @note Never throws.
     */
    [[nodiscard]] std::vector<EventCommand> take_event_commands() noexcept;

private:
    friend class ScriptRuntimeBuilder;

    struct State;

    ServerApi();
    void install(RuntimeContext& context) override;

    std::unique_ptr<State> _state;
};

} // namespace engine::script
