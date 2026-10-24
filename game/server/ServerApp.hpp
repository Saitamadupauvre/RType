#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Cli.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/SystemScheduler.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/IClock.hpp"
#include "engine/network/Audio.hpp"
#include "engine/network/Replicated.hpp"
#include "engine/network/Server.hpp"
#include "engine/script/ScriptRuntime.hpp"

namespace rtype::server {

class ServerApp {
public:
    ServerApp(const engine::core::time::IClock& clock, const ServerConfig& config,
              const std::filesystem::path& scripts = {});

    bool start(std::string& error);
    void tick(engine::core::time::Duration step);
    void run(const std::function<bool()>& should_stop);
    void stop();

    void apply_inputs(const std::vector<engine::network::PlayerInput>& inputs);
    void replicate();
    void publish_audio();

    [[nodiscard]] engine::core::EntityManager& registry() noexcept { return registry_; }
    [[nodiscard]] engine::core::SystemScheduler& systems() noexcept { return systems_; }
    [[nodiscard]] engine::core::event::EventBus& events() noexcept { return events_; }
    [[nodiscard]] engine::network::NetworkServer& network() noexcept { return network_; }
    [[nodiscard]] engine::script::ScriptRuntime& script() noexcept { return _runtime; }
    [[nodiscard]] const std::vector<engine::network::ReplicatedType>& types() const noexcept {
        return _types;
    }
    [[nodiscard]] const std::vector<engine::network::AudioName>& sounds() const noexcept {
        return _sounds;
    }
    [[nodiscard]] const std::vector<engine::network::AudioName>& musics() const noexcept {
        return _musics;
    }

private:
    std::reference_wrapper<const engine::core::time::IClock> clock_;
    engine::core::event::EventBus events_;
    engine::core::EntityManager registry_;
    engine::core::SystemScheduler systems_;
    engine::script::ScriptRuntime _runtime;
    std::vector<engine::network::ReplicatedType> _types;
    std::unordered_map<std::string, std::uint16_t> _type_ids;
    std::vector<engine::network::AudioName> _sounds;
    std::vector<engine::network::AudioName> _musics;
    std::unordered_map<std::string, std::uint16_t> _sound_ids;
    std::unordered_map<std::string, std::uint16_t> _music_ids;
    engine::network::NetworkServer network_;
};

} // namespace rtype::server
