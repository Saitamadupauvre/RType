#include "engine/network/Client.hpp"

#include <functional>
#include <random>
#include <string>
#include <utility>
#include <variant>

#include "engine/core/log/Log.hpp"
#include "engine/network/Events.hpp"

#include "event/GameEvents.hpp"
#include "session/ClientConnection.hpp"
#include "snapshot/SnapshotReceiver.hpp"
#include "socket/UdpSocket.hpp"
#include "stats/TrafficMeter.hpp"
#include "thread/NetworkIo.hpp"

namespace engine::network {

namespace {

Endpoint any_local_endpoint(const Endpoint& server) {
    const auto any_address = server.addr()->sa_family == AF_INET6 ? "::" : "0.0.0.0";
    return Endpoint::from_string(any_address, 0).value_or(Endpoint{});
}

std::uint32_t fresh_nonce() {
    std::random_device random;
    return random();
}

EntitySnapshot to_entity_snapshot(const EntityState& state) {
    return EntitySnapshot{.entity_id = state.entity_id,
                          .type_id = state.type_id,
                          .x = static_cast<float>(state.x),
                          .y = static_cast<float>(state.y),
                          .vx = static_cast<float>(state.vx),
                          .vy = static_cast<float>(state.vy),
                          .owner = state.owner,
                          .color = {.r = state.r, .g = state.g, .b = state.b, .a = state.a}};
}

} // namespace

struct NetworkClient::ImplNet {
    ImplNet(const core::time::IClock& game_clock, core::event::EventBus& game_events)
        : clock(game_clock), events(game_events) {}

    void send(std::optional<Outbound> outbound) {
        if (outbound) {
            io.send(std::move(*outbound));
        }
    }

    void report_stats(const ClientConnection& live) {
        const auto now = clock.get().now();
        if (now - last_report < stats_report_interval) {
            return;
        }
        last_report = now;
        core::log::info("network: server {}: {}", server_label, summarize(live.stats()));
    }

    void handle(ClientConnection& live, const Inbound& inbound) {
        const bool was_connected = live.state() == ConnectionState::connected;
        if (!live.handle(inbound) || live.state() != ConnectionState::connected) {
            return;
        }
        const auto& message = inbound.message.message;
        if (const auto* accept = std::get_if<ConnectAcceptPayload>(&message)) {
            if (!was_connected && accept->current_music != no_music) {
                events.get().publish(MusicChanged{.music_id = accept->current_music});
            }
        } else if (const auto* snapshot = std::get_if<SnapshotPayload>(&message)) {
            snapshots.receive(*snapshot);
        } else if (const auto* event = std::get_if<EventPayload>(&message)) {
            publish_received(events, *event);
        }
    }

    void close() {
        if (connection) {
            send(connection->disconnect());
        }
        io.stop();
    }

    void report_end() {
        const auto reason = connection ? connection->disconnect_reason() : std::nullopt;
        if (!reason || reported) {
            return;
        }
        reported = true;
        io.stop();
        events.get().publish(Disconnected{.reason = *reason});
    }

    std::reference_wrapper<const core::time::IClock> clock;
    std::reference_wrapper<core::event::EventBus> events;
    std::optional<ClientConnection> connection;
    SnapshotReceiver snapshots;
    std::string server_label;
    core::time::Duration last_report{};
    bool reported{false};
    NetworkIo io;
};

NetworkClient::NetworkClient(const core::time::IClock& clock, core::event::EventBus& events)
    : impl_(std::make_unique<ImplNet>(clock, events)) {}

NetworkClient::~NetworkClient() { impl_->close(); }

bool NetworkClient::connect(std::string_view server_ip, std::uint16_t port, std::error_code& ec) {
    ec.clear();
    if (impl_->connection) {
        ec = std::make_error_code(std::errc::already_connected);
        return false;
    }
    const auto server = Endpoint::from_string(server_ip, port);
    if (!server) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return false;
    }

    impl_->io.start(any_local_endpoint(*server), ec);
    if (ec) {
        return false;
    }
    impl_->connection.emplace(impl_->clock.get(), *server, fresh_nonce());
    impl_->server_label = server->to_string();
    impl_->last_report = impl_->clock.get().now();
    return true;
}

void NetworkClient::update() {
    auto& connection = impl_->connection;
    if (!connection || impl_->reported) {
        return;
    }
    auto& live = *connection;
    for (const auto& inbound : impl_->io.poll()) {
        impl_->handle(live, inbound);
    }
    impl_->send(live.update());
    impl_->report_stats(live);
    impl_->report_end();
}

void NetworkClient::send_input(std::uint8_t action_flags, std::int8_t move_x, std::int8_t move_y) {
    if (auto& connection = impl_->connection) {
        impl_->send(connection->send_input(
            InputPayload{.action_flags = action_flags, .move_x = move_x, .move_y = move_y}));
    }
}

void NetworkClient::disconnect() {
    impl_->close();
    impl_->report_end();
}

ConnectionState NetworkClient::state() const {
    const auto& connection = impl_->connection;
    return connection ? connection->state() : ConnectionState::disconnected;
}

std::uint32_t NetworkClient::player_id() const {
    const auto& connection = impl_->connection;
    return connection ? connection->player_id() : 0;
}

std::vector<ReplicatedType> NetworkClient::types() const {
    std::vector<ReplicatedType> types;
    const auto& connection = impl_->connection;
    if (!connection.has_value()) {
        return types;
    }
    for (const auto& entry : connection.value().type_table().entries()) {
        types.push_back({.type_id = entry.type_id, .name = entry.name});
    }
    return types;
}

std::vector<AudioName> NetworkClient::sounds() const {
    std::vector<AudioName> sounds;
    if (const auto& connection = impl_->connection) {
        for (const auto& entry : connection->type_table().sounds()) {
            sounds.push_back({.id = entry.type_id, .name = entry.name});
        }
    }
    return sounds;
}

std::vector<AudioName> NetworkClient::musics() const {
    std::vector<AudioName> musics;
    if (const auto& connection = impl_->connection) {
        for (const auto& entry : connection->type_table().musics()) {
            musics.push_back({.id = entry.type_id, .name = entry.name});
        }
    }
    return musics;
}

std::optional<std::uint32_t> NetworkClient::server_tick() const {
    return impl_->snapshots.server_tick();
}

std::vector<EntitySnapshot> NetworkClient::entities() const {
    std::vector<EntitySnapshot> entities;
    entities.reserve(impl_->snapshots.entities().size());
    for (const auto& state : impl_->snapshots.entities()) {
        entities.push_back(to_entity_snapshot(state));
    }
    return entities;
}

NetworkStats NetworkClient::stats() const {
    const auto& connection = impl_->connection;
    return connection ? connection->stats() : NetworkStats{};
}

} // namespace engine::network
