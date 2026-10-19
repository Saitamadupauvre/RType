#include "ServerNetwork.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

#include "snapshot/SnapshotBuilder.hpp"

namespace engine::network {

namespace {

void append(std::vector<Outbound>& outbound, std::vector<Outbound> more) {
    std::ranges::move(more, std::back_inserter(outbound));
}

} // namespace

ServerNetwork::ServerNetwork(const core::time::IClock& clock, core::event::EventBus& events,
                             ServerSessionsConfig config)
    : sessions_(clock, events, std::move(config)) {}

std::vector<Outbound> ServerNetwork::receive(const std::vector<Inbound>& inbound) {
    std::vector<Outbound> outbound;
    for (const auto& message : inbound) {
        append(outbound, sessions_.handle(message));
    }
    append(outbound, sessions_.drop_timed_out());
    return outbound;
}

void ServerNetwork::queue_event(const EventPayload& event) {
    append(queued_events_, sessions_.broadcast(event));
}

std::vector<Outbound> ServerNetwork::snapshot(core::EntityManager& registry) {
    ++server_tick_;
    auto outbound = std::exchange(queued_events_, {});
    if (sessions_.sessions().empty()) {
        return outbound;
    }
    for (auto& snapshot : build_snapshots(registry, server_tick_)) {
        append(outbound, sessions_.broadcast(std::move(snapshot)));
    }
    return outbound;
}

std::vector<Outbound> ServerNetwork::shutdown() {
    return sessions_.close_all(DisconnectReason::server_shutdown);
}

} // namespace engine::network
