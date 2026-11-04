#pragma once

#include <string_view>

#include "engine/script/ClientApi.hpp"
#include "lua/Sol.hpp"

namespace engine::script {

class ClientAudio {
public:
    void bind(sol::state& lua);

    [[nodiscard]] AudioSettings settings() const noexcept { return _settings; }
    void set_settings(AudioSettings settings) noexcept;

private:
    float& volume(sol::this_state state, const sol::object& kind, std::string_view function);
    void set_volume(sol::this_state state, const sol::object& kind, const sol::object& value);
    void set_muted(sol::this_state state, const sol::object& muted);

    AudioSettings _settings;
};

} // namespace engine::script
