#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "engine/core/time/IClock.hpp"
#include "engine/network/Export.hpp"
#include "engine/network/Stats.hpp"

#include "protocol/Messages.hpp"
#include "protocol/ProtocolHandler.hpp"

namespace engine::network {

inline constexpr core::time::Duration traffic_window = std::chrono::seconds(1);
inline constexpr core::time::Duration stats_report_interval = std::chrono::seconds(10);

[[nodiscard]] ENGINE_NETWORK_EXPORT std::size_t datagram_size(const Message& message);

[[nodiscard]] ENGINE_NETWORK_EXPORT std::string summarize(const NetworkStats& stats);

class ENGINE_NETWORK_EXPORT TrafficMeter {
public:
    explicit TrafficMeter(core::time::Duration now = {});

    void on_sent(const Message& message, core::time::Duration now);
    void on_received(const ReceivedMessage& received, core::time::Duration now);
    void on_round_trip(core::time::Duration round_trip) noexcept;

    [[nodiscard]] NetworkStats stats(core::time::Duration now) const;

private:
    struct Window {
        std::uint64_t packets_sent{};
        std::uint64_t packets_received{};
        std::uint64_t bytes_sent{};
        std::uint64_t bytes_received{};
    };

    void roll(core::time::Duration now) noexcept;
    void fill(NetworkStats& stats) const noexcept;
    void track_sequence(std::uint32_t sequence) noexcept;

    core::time::Duration window_start_;
    Window current_;
    Window last_;
    std::optional<std::uint32_t> newest_sequence_;
    std::uint64_t lost_{};
    std::uint64_t reordered_{};
    std::optional<core::time::Duration> round_trip_;
};

} // namespace engine::network
