#include "engine/network/Server.hpp"

#include <utility>

#include "server/ServerNetwork.hpp"
#include "socket/UdpSocket.hpp"
#include "thread/NetworkIo.hpp"

namespace engine::network {

struct NetworkServer::ImplNet {
    ImplNet(const core::time::IClock& clock, core::event::EventBus& events,
            ServerOptions server_options)
        : options(server_options),
          network(clock, events, ServerSessionsConfig{.max_players = server_options.max_players}) {}

    void send_all(std::vector<Outbound> outbound) {
        for (auto& message : outbound) {
            io.send(message.to, std::move(message.message));
        }
    }

    ServerOptions options;
    ServerNetwork network;
    NetworkIo io;
};

NetworkServer::NetworkServer(const core::time::IClock& clock, core::event::EventBus& events,
                             ServerOptions options)
    : impl_(std::make_unique<ImplNet>(clock, events, options)) {}

NetworkServer::~NetworkServer() = default;

bool NetworkServer::start(std::error_code& ec) {
    const auto any_address = Endpoint::from_string("0.0.0.0", impl_->options.port);
    impl_->io.start(any_address.value_or(Endpoint{}), ec);
    return !ec;
}

void NetworkServer::stop() {
    impl_->send_all(impl_->network.shutdown());
    impl_->io.stop();
}

void NetworkServer::receive() { impl_->send_all(impl_->network.receive(impl_->io.poll())); }

std::vector<PlayerInput> NetworkServer::inputs() const {
    std::vector<PlayerInput> inputs;
    for (const auto& session : impl_->network.sessions()) {
        inputs.push_back(PlayerInput{.player_id = session.player_id,
                                     .action_flags = session.input.action_flags,
                                     .move_x = session.input.move_x,
                                     .move_y = session.input.move_y});
    }
    return inputs;
}

void NetworkServer::send_snapshot(core::EntityManager& registry) {
    impl_->send_all(impl_->network.snapshot(registry));
}

std::uint16_t NetworkServer::port() const {
    std::error_code ec;
    const auto local = impl_->io.local_endpoint(ec);
    return ec ? impl_->options.port : local.port();
}

std::size_t NetworkServer::player_count() const { return impl_->network.sessions().size(); }

} // namespace engine::network
