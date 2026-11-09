#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"

#include "protocol/ProtocolHandler.hpp"
#include "thread/DatagramHandler.hpp"

namespace logger = engine::core::log;

namespace {

using engine::network::AddressedOutbound;
using engine::network::Bytes;
using engine::network::encode_outbound;
using engine::network::Endpoint;
using engine::network::handle_datagram;
using engine::network::InputPayload;
using engine::network::Message;
using engine::network::MessageType;
using engine::network::ProtocolHandler;

constexpr std::size_t header_version_offset = 4;
constexpr std::size_t header_payload_size_offset = 11;
constexpr std::size_t input_payload_size = 18;
constexpr std::size_t input_first_key_offset = engine::network::encoded_packet_header_size;
constexpr std::size_t input_mouse_offset = input_first_key_offset + 16;
constexpr std::size_t input_flags_offset = input_mouse_offset + 1;
constexpr std::uint32_t sample_sequence = 7;

Endpoint sample_endpoint() {
    auto endpoint = Endpoint::from_string("127.0.0.1", 4242);
    EXPECT_TRUE(endpoint.has_value());
    return endpoint.value_or(Endpoint{});
}

Message sample_message() {
    return InputPayload{.keys = {0x02}, .mouse_buttons = 0x01, .pointer = std::nullopt};
}

Bytes valid_datagram() {
    const ProtocolHandler handler;
    auto bytes = handler.encode(sample_message(), sample_sequence);
    EXPECT_TRUE(bytes.has_value());
    return std::move(bytes).value_or(Bytes{});
}

Bytes random_bytes(std::size_t size) {
    std::mt19937 generator(1234);
    std::uniform_int_distribution<int> byte(0, 255);
    Bytes bytes(size);
    for (auto& value : bytes) {
        value = static_cast<std::uint8_t>(byte(generator));
    }
    return bytes;
}

class DatagramHandler : public testing::Test {
protected:
    void expect_dropped_and_logged(const Bytes& datagram) {
        const auto inbound = handle_datagram(handler, datagram, sample_endpoint());

        EXPECT_FALSE(inbound.has_value());
        const auto records = sink->records();
        ASSERT_EQ(records.size(), 1U);
        EXPECT_EQ(records.front().level, logger::Level::Warn);
        EXPECT_NE(records.front().message.find("127.0.0.1:4242"), std::string::npos);
        EXPECT_NE(records.front().message.find(std::to_string(datagram.size()) + " bytes"),
                  std::string::npos);
    }

    ProtocolHandler handler;
    std::shared_ptr<logger::MemorySink> sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink scoped{sink, logger::Level::Trace};
};

} // namespace

TEST_F(DatagramHandler, ValidDatagramBecomesInboundFromSender) {
    const auto inbound = handle_datagram(handler, valid_datagram(), sample_endpoint());

    ASSERT_TRUE(inbound.has_value());
    EXPECT_EQ(inbound->from.to_string(), sample_endpoint().to_string());
    EXPECT_EQ(inbound->message.header.type, MessageType::input);
    EXPECT_EQ(inbound->message.header.sequence, sample_sequence);
    EXPECT_EQ(inbound->message.message, sample_message());
    EXPECT_TRUE(sink->records().empty());
}

TEST_F(DatagramHandler, DropsTruncatedDatagram) {
    auto datagram = valid_datagram();
    datagram.pop_back();

    expect_dropped_and_logged(datagram);
}

TEST_F(DatagramHandler, DropsEmptyDatagram) { expect_dropped_and_logged(Bytes{}); }

TEST_F(DatagramHandler, DropsBadMagic) {
    auto datagram = valid_datagram();
    datagram.front() ^= 0xFF;

    expect_dropped_and_logged(datagram);
}

TEST_F(DatagramHandler, DropsWrongVersion) {
    auto datagram = valid_datagram();
    datagram[header_version_offset + 1] ^= 0xFF;

    expect_dropped_and_logged(datagram);
}

TEST_F(DatagramHandler, DropsRandomBytes) { expect_dropped_and_logged(random_bytes(64)); }

TEST_F(DatagramHandler, DropsInputHoldingTheUnknownKey) {
    auto datagram = valid_datagram();
    datagram[input_first_key_offset] = 0x03;

    expect_dropped_and_logged(datagram);
}

TEST_F(DatagramHandler, DropsInputWithUnknownMouseButtons) {
    auto datagram = valid_datagram();
    datagram[input_mouse_offset] = 0x80;

    expect_dropped_and_logged(datagram);
}

TEST_F(DatagramHandler, DropsInputWithUnknownFlags) {
    auto datagram = valid_datagram();
    datagram[input_flags_offset] = 0x80;

    expect_dropped_and_logged(datagram);
}

TEST_F(DatagramHandler, DropsInputWithWrongPayloadSize) {
    auto datagram = valid_datagram();
    datagram.push_back(0);
    datagram[header_payload_size_offset + 1] = static_cast<std::uint8_t>(input_payload_size + 1);

    expect_dropped_and_logged(datagram);
}

TEST_F(DatagramHandler, EncodesValidOutboundWithSequence) {
    const AddressedOutbound outbound{.to = sample_endpoint(), .message = sample_message()};

    const auto bytes = encode_outbound(handler, outbound, sample_sequence);

    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(*bytes, valid_datagram());
    EXPECT_TRUE(sink->records().empty());
}

TEST_F(DatagramHandler, DropsAndLogsInvalidOutboundPayload) {
    const AddressedOutbound outbound{
        .to = sample_endpoint(),
        .message = InputPayload{.keys = {0x01}, .mouse_buttons = 0, .pointer = std::nullopt}};

    const auto bytes = encode_outbound(handler, outbound, sample_sequence);

    EXPECT_FALSE(bytes.has_value());
    const auto records = sink->records();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records.front().level, logger::Level::Warn);
    EXPECT_NE(records.front().message.find("127.0.0.1:4242"), std::string::npos);
}
