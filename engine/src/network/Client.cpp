#include "engine/network/Client.hpp"

#include <algorithm>
#include <functional>
#include <random>
#include <string>
#include <utility>
#include <variant>

#include "engine/core/log/Log.hpp"
#include "engine/network/Events.hpp"

#include "event/GameEvents.hpp"
#include "input/InputCodec.hpp"
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

EntitySnapshot to_entity_snapshot(const EntityState& state, const ReceivedStrings& strings) {
    EntitySnapshot snapshot{.entity_id = state.entity_id, .type_id = state.type_id, .fields = {}};
    snapshot.fields.reserve(state.fields.size());
    for (const FieldState& field : state.fields) {
        snapshot.fields.push_back(
            ReplicatedValue{.field = strings.field(field.field_id), .value = field.value});
    }
    return snapshot;
}

} // namespace

const core::FieldValue* EntitySnapshot::find(std::string_view component,
                                             std::string_view field) const {
    const auto found = std::ranges::find_if(fields, [&](const ReplicatedValue& replicated) {
        return replicated.field && replicated.field.component->name() == component &&
               replicated.field.field->name() == field;
    });
    return found == fields.end() ? nullptr : &found->value;
}

struct NetworkClient::ImplNet {
    ImplNet(const core::time::IClock& game_clock, core::event::EventBus& game_events,
            const core::FieldRegistry& known_fields)
        : clock(game_clock), events(game_events), fields(known_fields) {}

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
            publish_received(events, *event, live.strings().names());
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
    std::reference_wrapper<const core::FieldRegistry> fields;
    std::optional<ClientConnection> connection;
    SnapshotReceiver snapshots;
    std::string server_label;
    core::time::Duration last_report{};
    bool reported{false};
    NetworkIo io;
};

NetworkClient::NetworkClient(const core::time::IClock& clock, core::event::EventBus& events,
                             const core::FieldRegistry& fields)
    : impl_(std::make_unique<ImplNet>(clock, events, fields)) {}

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
    impl_->connection.emplace(impl_->clock.get(), impl_->fields.get(), *server, fresh_nonce());
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

void NetworkClient::send_input(const core::InputState& input) {
    if (auto& connection = impl_->connection) {
        impl_->send(connection->send_input(to_payload(input)));
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

StringTable NetworkClient::strings() const {
    const auto& connection = impl_->connection;
    return connection ? connection->strings().table() : StringTable{};
}

std::optional<std::uint32_t> NetworkClient::server_tick() const {
    return impl_->snapshots.server_tick();
}

std::vector<EntitySnapshot> NetworkClient::entities() const {
    std::vector<EntitySnapshot> entities;
    const auto& connection = impl_->connection;
    if (!connection) {
        return entities;
    }
    entities.reserve(impl_->snapshots.entities().size());
    for (const auto& state : impl_->snapshots.entities()) {
        entities.push_back(to_entity_snapshot(state, connection->strings()));
    }
    return entities;
}

NetworkStats NetworkClient::stats() const {
    const auto& connection = impl_->connection;
    return connection ? connection->stats() : NetworkStats{};
}

} // namespace engine::network
