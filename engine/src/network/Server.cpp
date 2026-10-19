#include "engine/network/Server.hpp"

#include <utility>

#include "engine/core/log/Log.hpp"
#include "engine/network/Events.hpp"

#include "event/GameEvents.hpp"
#include "protocol/Validation.hpp"
#include "server/ServerNetwork.hpp"
#include "socket/UdpSocket.hpp"
#include "thread/NetworkIo.hpp"

namespace engine::network {

namespace {

std::vector<TypeEntry> to_type_entries(const std::vector<ReplicatedType>& types) {
    std::vector<TypeEntry> entries;
    entries.reserve(types.size());
    for (const auto& type : types) {
        entries.push_back(TypeEntry{.type_id = type.type_id, .name = type.name});
    }
    return entries;
}

} // namespace

struct NetworkServer::ImplNet {
    ImplNet(const core::time::IClock& clock, core::event::EventBus& game_events,
            ServerOptions server_options)
        : options(std::move(server_options)), events(game_events),
          network(clock, game_events,
                  ServerSessionsConfig{.max_players = options.max_players,
                                       .types = to_type_entries(options.types)}) {
        forward<EntitySpawned>();
        forward<EntityDestroyed>();
        forward<ShotFired>();
        forward<SoundPlayed>();
    }

    ~ImplNet() {
        for (const auto subscription : subscriptions) {
            events.get().unsubscribe(subscription);
        }
    }

    ImplNet(const ImplNet&) = delete;
    ImplNet& operator=(const ImplNet&) = delete;
    ImplNet(ImplNet&&) = delete;
    ImplNet& operator=(ImplNet&&) = delete;

    template <typename Event> void forward() {
        subscriptions.push_back(events.get().subscribe<Event>(
            [this](const Event& event) { network.queue_event(to_payload(event)); }));
    }

    void send_all(std::vector<Outbound> outbound) {
        for (auto& message : outbound) {
            io.send(std::move(message));
        }
    }

    ServerOptions options;
    std::reference_wrapper<core::event::EventBus> events;
    std::vector<core::event::Subscription> subscriptions;
    ServerNetwork network;
    NetworkIo io;
};

NetworkServer::NetworkServer(const core::time::IClock& clock, core::event::EventBus& events,
                             ServerOptions options)
    : impl_(std::make_unique<ImplNet>(clock, events, std::move(options))) {}

NetworkServer::~NetworkServer() = default;

bool NetworkServer::start(std::error_code& ec) {
    ec.clear();
    if (!is_valid(ConnectAcceptPayload{.types = to_type_entries(impl_->options.types)})) {
        core::log::error("network: invalid type table: ids and names must be unique, names 1 "
                         "to 63 bytes");
        ec = std::make_error_code(std::errc::invalid_argument);
        return false;
    }
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

std::vector<PlayerStats> NetworkServer::stats() const { return impl_->network.stats(); }

} // namespace engine::network
