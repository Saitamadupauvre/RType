#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "engine/core/ecs/Entity.hpp"
#include "engine/core/input/InputState.hpp"
#include "engine/core/math/Camera.hpp"
#include "engine/core/math/Vec2.hpp"
#include "engine/script/ApiSet.hpp"
#include "engine/script/EventCommand.hpp"
#include "engine/script/Export.hpp"
#include "engine/script/HudCommand.hpp"

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
 * @brief Camera chosen by client scripts through `camera`, local to this client.
 */
struct CameraOverride {
    enum class Kind : std::uint8_t {
        /** @brief No override: the scene camera, or the default view, is used. */
        None,
        /** @brief Show @ref view as is. */
        Fixed,
        /** @brief Center on the replicated entity @ref entity with the zoom of @ref view. */
        Follow,
    };

    Kind kind{Kind::None};
    /** @brief Center and zoom for Fixed; only the zoom is used for Follow. */
    core::CameraView view;
    /** @brief Entity followed, as read from `self.id` on the server. */
    core::Entity::Id entity{0};

    bool operator==(const CameraOverride&) const = default;
};

/**
 * @brief API set of the presentation side: local input, audio levels, events received from the
 * server, local entities and the HUD.
 *
 * Reads the `client = { ... }` section of declarations and of `main.lua`, and adds these Lua
 * globals:
 * - `on(name, handler)` takes an event declared with `event "name" { ... }` and a function; the
 *   function is called with a table of the event's arguments every time dispatch() receives
 *   that event. An undeclared event or a handler that is not a function raises a Lua error.
 *   The built-in `scene_loaded` event (argument `name`), sent when the server switches scene,
 *   also calls `on_scene_loaded(name)` from `main.lua` if it defines one, before the handlers.
 * - `input` is the local input given to set_input(): `input:key(name)`, `input:mouse(name)` and
 *   `input:pointer()` read it like an entity's `self.input` on the server, and
 *   `input:pressed(name)` is true only when the key went down since the previous set_input().
 *   An unknown key or button name raises a Lua error.
 * - `audio.muted()`, `audio.set_muted(muted)`, `audio.volume(kind)` and
 *   `audio.set_volume(kind, volume)` read and change audio_settings(); `kind` is `"music"` or
 *   `"effects"` and `volume` a number from 0 to 1. Any other argument raises a Lua error and
 *   changes nothing.
 * - `camera.set(x, y [, zoom])` shows the world point (x, y) at the center of the screen,
 *   `camera.follow(id [, zoom])` centers on the replicated entity whose `self.id` is `id`, and
 *   `camera.reset()` goes back to the scene camera; they only change camera_override(), so other
 *   clients are not affected. A missing zoom keeps the zoom of the current view.
 *   `camera.screen_to_world(x, y)` returns the world point under a screen point, such as
 *   `input:pointer()`, through the view given to set_camera_view(). A coordinate that is not a
 *   finite number, a zoom that is not positive or an id that is not a non-negative integer raises
 *   a Lua error and changes nothing.
 * - `spawn_local(type [, overrides])` spawns a declared entity, with its `client` section, into
 *   the world given to ScriptRuntime::attach_world(), exactly like `spawn` on the server: the
 *   host never replicates it. An unknown type is logged and returns nil.
 * - `draw.text(text, x, y, size [, color])`, `draw.rect(x, y, w, h [, color [, filled]])` and
 *   `draw.sprite(texture, x, y [, { scale, rotation, color }])` queue a HudCommand in screen
 *   space; they only work while draw_hud() runs the `draw_hud()` callback of `main.lua`. A
 *   coordinate that is not a finite number, a negative size, a text longer than
 *   `max_hud_text_length` bytes, an undeclared texture or a call outside `draw_hud()` raises a Lua
 *   error and queues nothing.
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

    /**
     * @return The camera chosen by scripts: Kind::None until they call `camera.set` or
     * `camera.follow`, and again after `camera.reset`.
     */
    [[nodiscard]] CameraOverride camera_override() const noexcept;

    /**
     * @brief Sets the view `camera.screen_to_world` converts through, and the zoom kept by
     * `camera.set` and `camera.follow` when they get none.
     *
     * Typically called every frame with the view being drawn. Until the first call, the view is
     * core::default_view() of a 1920x1080 viewport.
     *
     * @param view View being drawn; its zoom must be strictly positive.
     * @param viewport Size of the screen in virtual units.
     */
    void set_camera_view(core::CameraView view, core::Vec2 viewport) noexcept;

    /**
     * @brief Calls `draw_hud()` from `main.lua` and returns what it drew, in call order.
     *
     * The call is protected: an error is logged and the commands queued before it are still
     * returned. At most `max_hud_commands` are kept; later ones are dropped and logged once.
     *
     * @return The HUD of this frame, empty when `main.lua` defines no `draw_hud`.
     * @note Never throws. Typically called once per frame, after the world is drawn.
     */
    [[nodiscard]] std::vector<HudCommand> draw_hud() noexcept;

private:
    friend class ScriptRuntimeBuilder;

    struct State;

    ClientApi();
    void install(RuntimeContext& context) override;

    std::unique_ptr<State> _state;
};

} // namespace engine::script
