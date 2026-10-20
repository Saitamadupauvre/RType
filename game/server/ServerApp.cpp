#include "ServerApp.hpp"

#include <chrono>
#include <system_error>
#include <thread>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/time/FixedStepLoop.hpp"
#include "engine/network/Replicated.hpp"

namespace rtype::server {

namespace {

using namespace std::chrono_literals;

constexpr auto idle_between_frames = 1ms;

template <typename Component> void register_if_missing(engine::core::EntityManager& registry) {
    if (!registry.is_registered<Component>()) {
        registry.register_component<Component>();
    }
}

} // namespace

ServerApp::ServerApp(const engine::core::time::IClock& clock, const ServerConfig& config)
    : clock_(clock), network_(clock, events_,
                              engine::network::ServerOptions{.port = config.port,
                                                             .max_players = config.max_players}) {
    register_if_missing<engine::network::Replicated>(registry_);
    register_if_missing<engine::core::Position>(registry_);
    register_if_missing<engine::core::Velocity>(registry_);
}

bool ServerApp::start(std::string& error) {
    std::error_code ec;
    if (!network_.start(ec)) {
        error = "cannot listen on port " + std::to_string(network_.port()) + ": " + ec.message();
        return false;
    }
    return true;
}

void ServerApp::tick(engine::core::time::Duration step) {
    network_.receive();
    systems_.run(registry_, std::chrono::duration<float>(step).count());
    network_.send_snapshot(registry_);
}

void ServerApp::run(const std::function<bool()>& should_stop) {
    engine::core::time::FixedStepLoop loop(clock_.get());
    while (!should_stop()) {
        loop.run_frame([this](engine::core::time::Duration step) { tick(step); });
        std::this_thread::sleep_for(idle_between_frames);
    }
    stop();
}

void ServerApp::stop() { network_.stop(); }

} // namespace rtype::server
