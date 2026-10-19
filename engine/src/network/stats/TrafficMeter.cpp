#include "TrafficMeter.hpp"

#include <format>
#include <string_view>

#include "protocol/Protocol.hpp"
#include "protocol/Sequence.hpp"

namespace engine::network {

namespace {

std::string format_round_trip(const std::optional<core::time::Duration>& round_trip) {
    if (!round_trip) {
        return "n/a";
    }
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(*round_trip);
    return std::format("{} ms", milliseconds.count());
}

std::string format_rate(std::string_view direction, std::uint64_t packets, std::uint64_t bytes) {
    return std::format("{} {} pkt/s {} B/s", direction, packets, bytes);
}

std::string format_sequence_health(std::uint64_t lost, std::uint64_t reordered) {
    return std::format("lost {}, reordered {}", lost, reordered);
}

} // namespace

std::size_t datagram_size(const Message& message) {
    const auto bytes = ProtocolHandler{}.encode(message, 0);
    return bytes ? bytes->size() : 0;
}

std::string summarize(const NetworkStats& stats) {
    return std::format(
        "{}, {}, {}, rtt {}",
        format_rate("out", stats.packets_sent_per_second, stats.bytes_sent_per_second),
        format_rate("in", stats.packets_received_per_second, stats.bytes_received_per_second),
        format_sequence_health(stats.packets_lost, stats.packets_reordered),
        format_round_trip(stats.round_trip_time));
}

TrafficMeter::TrafficMeter(core::time::Duration now) : window_start_(now) {}

void TrafficMeter::on_sent(const Message& message, core::time::Duration now) {
    roll(now);
    ++current_.packets_sent;
    current_.bytes_sent += datagram_size(message);
}

void TrafficMeter::on_received(const ReceivedMessage& received, core::time::Duration now) {
    roll(now);
    ++current_.packets_received;
    current_.bytes_received += encoded_packet_header_size + received.header.payload_size;
    track_sequence(received.header.sequence);
}

void TrafficMeter::on_round_trip(core::time::Duration round_trip) noexcept {
    round_trip_ = round_trip;
}

NetworkStats TrafficMeter::stats(core::time::Duration now) const {
    auto rolled = *this;
    rolled.roll(now);
    NetworkStats stats;
    rolled.fill(stats);
    return stats;
}

void TrafficMeter::fill(NetworkStats& stats) const noexcept {
    stats.packets_sent_per_second = last_.packets_sent;
    stats.packets_received_per_second = last_.packets_received;
    stats.bytes_sent_per_second = last_.bytes_sent;
    stats.bytes_received_per_second = last_.bytes_received;
    stats.packets_lost = lost_;
    stats.packets_reordered = reordered_;
    stats.round_trip_time = round_trip_;
}

void TrafficMeter::roll(core::time::Duration now) noexcept {
    if (now - window_start_ < traffic_window) {
        return;
    }
    const auto elapsed_windows = (now - window_start_) / traffic_window;
    last_ = elapsed_windows == 1 ? current_ : Window{};
    current_ = Window{};
    window_start_ += traffic_window * elapsed_windows;
}

void TrafficMeter::track_sequence(std::uint32_t sequence) noexcept {
    if (!newest_sequence_) {
        newest_sequence_ = sequence;
        return;
    }
    if (is_newer_sequence(sequence, *newest_sequence_)) {
        lost_ += sequence - *newest_sequence_ - 1;
        newest_sequence_ = sequence;
        return;
    }
    if (sequence == *newest_sequence_) {
        return;
    }
    ++reordered_;
    if (lost_ > 0) {
        --lost_;
    }
}

} // namespace engine::network
