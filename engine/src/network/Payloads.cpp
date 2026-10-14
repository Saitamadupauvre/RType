#include "Payloads.hpp"

#include <utility>

#include "ByteReader.hpp"
#include "ByteWriter.hpp"

namespace engine::network {

namespace {

constexpr std::uint8_t known_action_flags = 0x01;

bool is_unit_axis(std::int8_t value) noexcept { return value >= -1 && value <= 1; }

bool is_valid(const InputPayload& payload) noexcept {
    return (payload.action_flags & ~known_action_flags) == 0 && is_unit_axis(payload.move_x) &&
           is_unit_axis(payload.move_y);
}

bool is_valid(const DisconnectPayload& payload) noexcept {
    return static_cast<std::uint8_t>(payload.reason) <=
           static_cast<std::uint8_t>(DisconnectReason::protocol_error);
}

} // namespace

EncodedPayload encode_payload(const InputPayload& payload) {
    if (!is_valid(payload)) {
        return std::nullopt;
    }

    ByteWriter writer;
    writer.write_u8(payload.action_flags);
    writer.write_i8(payload.move_x);
    writer.write_i8(payload.move_y);
    return std::move(writer).take();
}

std::optional<InputPayload> decode_input(std::span<const std::uint8_t> payload) {
    ByteReader reader(payload);
    const auto action_flags = reader.read_u8();
    const auto move_x = reader.read_i8();
    const auto move_y = reader.read_i8();
    if (!action_flags || !move_x || !move_y || !reader.at_end()) {
        return std::nullopt;
    }

    const InputPayload input{.action_flags = *action_flags, .move_x = *move_x, .move_y = *move_y};
    if (!is_valid(input)) {
        return std::nullopt;
    }
    return input;
}

EncodedPayload encode_payload(const ConnectRequestPayload& payload) {
    ByteWriter writer;
    writer.write_u32(payload.client_nonce);
    return std::move(writer).take();
}

std::optional<ConnectRequestPayload> decode_connect_request(std::span<const std::uint8_t> payload) {
    ByteReader reader(payload);
    const auto client_nonce = reader.read_u32();
    if (!client_nonce || !reader.at_end()) {
        return std::nullopt;
    }
    return ConnectRequestPayload{.client_nonce = *client_nonce};
}

EncodedPayload encode_payload(const HeartbeatPayload&) { return Bytes{}; }

std::optional<HeartbeatPayload> decode_heartbeat(std::span<const std::uint8_t> payload) {
    if (!payload.empty()) {
        return std::nullopt;
    }
    return HeartbeatPayload{};
}

EncodedPayload encode_payload(const DisconnectPayload& payload) {
    if (!is_valid(payload)) {
        return std::nullopt;
    }

    ByteWriter writer;
    writer.write_u8(static_cast<std::uint8_t>(payload.reason));
    return std::move(writer).take();
}

std::optional<DisconnectPayload> decode_disconnect(std::span<const std::uint8_t> payload) {
    ByteReader reader(payload);
    const auto reason = reader.read_u8();
    if (!reason || !reader.at_end()) {
        return std::nullopt;
    }

    const DisconnectPayload disconnect{.reason = static_cast<DisconnectReason>(*reason)};
    if (!is_valid(disconnect)) {
        return std::nullopt;
    }
    return disconnect;
}

} // namespace engine::network
