#include "ServerNetwork.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

#include "engine/core/log/Log.hpp"

#include "snapshot/SnapshotBuilder.hpp"
#include "stats/TrafficMeter.hpp"

namespace engine::network {

namespace {

void append(std::vector<Outbound>& outbound, std::vector<Outbound> more) {
    std::ranges::move(more, std::back_inserter(outbound));
}

} // namespace

ServerNetwork::ServerNetwork(const core::time::IClock& clock, core::event::EventBus& events,
                             const core::FieldRegistry& fields, ServerSessionsConfig config)
    : clock_(clock), last_report_(clock.now()), _replicated(fields, config.strings),
      sessions_(clock, events, std::move(config)) {}

std::vector<Outbound> ServerNetwork::receive(const std::vector<Inbound>& inbound) {
    std::vector<Outbound> outbound;
    for (const auto& message : inbound) {
        append(outbound, sessions_.handle(message));
    }
    append(outbound, sessions_.drop_timed_out());
    report_stats();
    return outbound;
}

void ServerNetwork::report_stats() {
    const auto now = clock_.get().now();
    if (now - last_report_ < stats_report_interval) {
        return;
    }
    last_report_ = now;
    for (const auto& player : sessions_.stats()) {
        core::log::info("network: player {}: {}", player.player_id, summarize(player.network));
    }
}

void ServerNetwork::queue_event(const EventPayload& event) {
    append(queued_events_, sessions_.broadcast(event));
}

std::vector<Outbound> ServerNetwork::snapshot(core::EntityManager& world) {
    ++server_tick_;
    auto outbound = std::exchange(queued_events_, {});
    if (sessions_.sessions().empty()) {
        return outbound;
    }
    for (auto& snapshot : build_snapshots(world, _replicated, server_tick_)) {
        append(outbound, sessions_.broadcast(std::move(snapshot)));
    }
    return outbound;
}

std::vector<Outbound> ServerNetwork::shutdown() {
    return sessions_.close_all(DisconnectReason::server_shutdown);
}

} // namespace engine::network
