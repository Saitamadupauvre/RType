#include "engine/network/Server.hpp"

#include <algorithm>
#include <utility>

#include "engine/core/log/Log.hpp"
#include "engine/network/Events.hpp"

#include "event/GameEvents.hpp"
#include "input/InputCodec.hpp"
#include "protocol/Validation.hpp"
#include "server/ServerNetwork.hpp"
#include "transport/Link.hpp"

namespace engine::network {

namespace {

ServerSessionsConfig sessions_config(const ServerOptions& options, const Link& link) {
    return ServerSessionsConfig{
        .max_players = options.max_players,
        .strings = options.strings,
        .describe_peer = [&link](PeerId peer) { return link.describe(peer); }};
}

} // namespace

struct NetworkServer::ImplNet {
    ImplNet(const core::time::IClock& clock, core::event::EventBus& game_events,
            const core::FieldRegistry& fields, Link& transport, ServerOptions server_options)
        : options(std::move(server_options)), events(game_events), link(transport),
          network(clock, game_events, fields, sessions_config(options, transport)) {
        forward<SoundPlayed>();
        forward<MusicChanged>();
        subscriptions.push_back(events.get().subscribe<MusicChanged>(
            [this](const MusicChanged& changed) { network.set_current_music(changed.music_id); }));
        subscriptions.push_back(events.get().subscribe<NamedEvent>([this](const NamedEvent& event) {
            if (auto payload = to_payload(event, options.strings)) {
                network.queue_event(*payload);
            }
        }));
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
            link.get().send(std::move(message));
        }
    }

    [[nodiscard]] bool has_session(PeerId peer) const {
        return std::ranges::find(network.sessions(), peer, &Session::peer) !=
               network.sessions().end();
    }

    void receive() {
        std::vector<PeerId> seen;
        for (const auto& session : network.sessions()) {
            seen.push_back(session.peer);
        }
        const auto inbound = link.get().poll();
        for (const auto& message : inbound) {
            seen.push_back(message.from);
        }
        send_all(network.receive(inbound));
        std::ranges::sort(seen);
        const auto duplicates = std::ranges::unique(seen);
        seen.erase(duplicates.begin(), duplicates.end());
        for (const auto peer : seen) {
            if (!has_session(peer)) {
                link.get().forget(peer);
            }
        }
    }

    ServerOptions options;
    std::reference_wrapper<core::event::EventBus> events;
    std::vector<core::event::Subscription> subscriptions;
    std::reference_wrapper<Link> link;
    ServerNetwork network;
};

NetworkServer::NetworkServer(const core::time::IClock& clock, core::event::EventBus& events,
                             const core::FieldRegistry& fields, ServerTransport& transport,
                             ServerOptions options)
    : impl_(
          std::make_unique<ImplNet>(clock, events, fields, transport.link(), std::move(options))) {}

NetworkServer::~NetworkServer() { stop(); }

bool NetworkServer::start(std::error_code& ec) {
    ec.clear();
    const auto& replicated = impl_->network.replicated();
    if (!replicated.error().empty()) {
        core::log::error("network: {}", replicated.error());
        ec = std::make_error_code(std::errc::invalid_argument);
        return false;
    }
    if (!is_valid(ConnectAcceptPayload{.strings = impl_->network.strings()})) {
        core::log::error("network: the string table does not fit in one packet");
        ec = std::make_error_code(std::errc::invalid_argument);
        return false;
    }
    impl_->link.get().open(ec);
    return !ec;
}

void NetworkServer::stop() {
    impl_->send_all(impl_->network.shutdown());
    impl_->link.get().close();
}

void NetworkServer::receive() { impl_->receive(); }

std::vector<PlayerInput> NetworkServer::inputs() const {
    std::vector<PlayerInput> inputs;
    for (const auto& session : impl_->network.sessions()) {
        inputs.push_back(
            PlayerInput{.player_id = session.player_id, .input = to_input_state(session.input)});
    }
    return inputs;
}

void NetworkServer::send_snapshot(core::EntityManager& world) {
    impl_->send_all(impl_->network.snapshot(world));
}

std::size_t NetworkServer::player_count() const { return impl_->network.sessions().size(); }

std::vector<PlayerStats> NetworkServer::stats() const { return impl_->network.stats(); }

} // namespace engine::network
