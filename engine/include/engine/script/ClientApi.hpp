#pragma once

#include <memory>
#include <string_view>

#include "engine/core/InputState.hpp"
#include "engine/script/ApiSet.hpp"
#include "engine/script/EventCommand.hpp"
#include "engine/script/Export.hpp"

namespace engine::script {

class ScriptRuntimeBuilder;

/**
 * @brief Audio levels of a client, read and changed by scripts through `audio`.
 */
struct AudioSettings {
    /** @brief Music volume, from 0 to 1. */
    float music_volume{1.F};
    /** @brief Sound effects volume, from 0 to 1. */
    float effects_volume{1.F};
    /** @brief Whether every sound and the music are silenced, whatever the volumes. */
    bool muted{false};

    bool operator==(const AudioSettings&) const = default;
};

/**
 * @brief API set of the presentation side: local input, audio levels and events received from
 * the server.
 *
 * Reads the `client = { ... }` section of declarations and of `main.lua`, and adds these Lua
 * globals:
 * - `on(name, handler)` takes an event declared with `event "name" { ... }` and a function; the
 *   function is called with a table of the event's arguments every time dispatch() receives
 *   that event. An undeclared event or a handler that is not a function raises a Lua error.
 * - `input` is the local input given to set_input(): `input:key(name)`, `input:mouse(name)` and
 *   `input:pointer()` read it like an entity's `self.input` on the server, and
 *   `input:pressed(name)` is true only when the key went down since the previous set_input().
 *   An unknown key or button name raises a Lua error.
 * - `audio.muted()`, `audio.set_muted(muted)`, `audio.volume(kind)` and
 *   `audio.set_volume(kind, volume)` read and change audio_settings(); `kind` is `"music"` or
 *   `"effects"` and `volume` a number from 0 to 1. Any other argument raises a Lua error and
 *   changes nothing.
 *
 * @note Not thread safe. Created by ScriptRuntimeBuilder::with(), reached with
 * ScriptRuntime::api(), destroyed with its runtime.
 */
class ENGINE_SCRIPT_EXPORT ClientApi final : public ApiSet {
public:
    ~ClientApi() override;

    ClientApi(const ClientApi&) = delete;
    ClientApi& operator=(const ClientApi&) = delete;
    ClientApi(ClientApi&&) = delete;
    ClientApi& operator=(ClientApi&&) = delete;

    /** @brief Returns `"client"`. */
    [[nodiscard]] std::string_view section() const noexcept override;

    /**
     * @brief Calls every handler registered with `on(event.name, ...)`, in registration order.
     *
     * Each handler gets a new table mapping every argument key to its value, converted like an
     * entity field. Every call is protected: an error is logged and the other handlers still run.
     *
     * @param event Event received from the server, typically converted from a NamedEvent.
     * @return true if every handler ran to completion, including when there is none; false if a
     * handler raised an error.
     * @note Never throws.
     */
    bool dispatch(const EventCommand& event) noexcept;

    /**
     * @brief Sets the input read through `input` until the next call.
     *
     * The input given to the previous call becomes the one `input:pressed()` compares with.
     * Typically called once per frame, before ScriptRuntime::update().
     *
     * @param input Local input of this frame.
     */
    void set_input(core::InputState input) noexcept;

    /** @return The audio levels, as last set by set_audio_settings() or by scripts. */
    [[nodiscard]] AudioSettings audio_settings() const noexcept;

    /**
     * @brief Replaces the audio levels scripts read and change.
     *
     * @param settings New levels; volumes are clamped to [0, 1].
     */
    void set_audio_settings(AudioSettings settings) noexcept;

private:
    friend class ScriptRuntimeBuilder;

    struct State;

    ClientApi();
    void install(RuntimeContext& context) override;

    std::unique_ptr<State> _state;
};

} // namespace engine::script
