#include <array>
#include <cstdint>
#include <span>

#include <gtest/gtest.h>

#include "bytes/ByteReader.hpp"

using engine::network::ByteReader;

TEST(ByteReader, ReadsUnsignedIntegersInBigEndianOrder) {
    const std::array<std::uint8_t, 7> data{0xAB, 0x01, 0x02, 0x0A, 0x0B, 0x0C, 0x0D};
    ByteReader reader(data);

    EXPECT_EQ(reader.read_u8(), 0xAB);
    EXPECT_EQ(reader.read_u16(), 0x0102);
    EXPECT_EQ(reader.read_u32(), 0x0A0B'0C0DU);
    EXPECT_TRUE(reader.at_end());
}

TEST(ByteReader, ReadsSignedIntegersAsTwosComplement) {
    const std::array<std::uint8_t, 6> data{0xFF, 0x01, 0xFF, 0xFE, 0x80, 0x00};
    ByteReader reader(data);

    EXPECT_EQ(reader.read_i8(), -1);
    EXPECT_EQ(reader.read_i8(), 1);
    EXPECT_EQ(reader.read_i16(), -2);
    EXPECT_EQ(reader.read_i16(), -32768);
}

TEST(ByteReader, ReadsMaximumUnsignedValues) {
    const std::array<std::uint8_t, 4> data{0xFF, 0xFF, 0xFF, 0xFF};
    ByteReader reader(data);

    EXPECT_EQ(reader.read_u32(), 0xFFFF'FFFFU);
}

TEST(ByteReader, RejectsReadPastEnd) {
    const std::array<std::uint8_t, 3> data{0x01, 0x02, 0x03};
    ByteReader reader(data);

    EXPECT_FALSE(reader.read_u32().has_value());
}

TEST(ByteReader, FailedReadDoesNotConsumeBytes) {
    const std::array<std::uint8_t, 3> data{0x01, 0x02, 0x03};
    ByteReader reader(data);

    ASSERT_FALSE(reader.read_u32().has_value());

    EXPECT_EQ(reader.remaining(), 3U);
    EXPECT_EQ(reader.read_u16(), 0x0102);
    EXPECT_EQ(reader.read_u8(), 0x03);
}

TEST(ByteReader, RejectsEveryReadOnEmptyData) {
    ByteReader reader(std::span<const std::uint8_t>{});

    EXPECT_TRUE(reader.at_end());
    EXPECT_FALSE(reader.read_u8().has_value());
    EXPECT_FALSE(reader.read_i8().has_value());
    EXPECT_FALSE(reader.read_u16().has_value());
    EXPECT_FALSE(reader.read_i16().has_value());
    EXPECT_FALSE(reader.read_u32().has_value());
    EXPECT_FALSE(reader.read_bytes(1).has_value());
}

TEST(ByteReader, ReadsByteRangeWithoutCopying) {
    const std::array<std::uint8_t, 4> data{'a', 'b', 'c', 'd'};
    ByteReader reader(data);
    ASSERT_TRUE(reader.read_u8().has_value());

    const auto bytes = reader.read_bytes(2);

    ASSERT_TRUE(bytes.has_value());
    const auto range = bytes.value_or(std::span<const std::uint8_t>{});
    EXPECT_EQ(range.data(), data.data() + 1);
    EXPECT_EQ(range.size(), 2U);
    EXPECT_EQ(reader.remaining(), 1U);
}

TEST(ByteReader, ReadsEmptyByteRange) {
    const std::array<std::uint8_t, 1> data{0x01};
    ByteReader reader(data);

    const auto bytes = reader.read_bytes(0);

    ASSERT_TRUE(bytes.has_value());
    EXPECT_TRUE(bytes.value_or(std::span<const std::uint8_t>{}).empty());
    EXPECT_EQ(reader.remaining(), 1U);
}

TEST(ByteReader, RejectsByteRangePastEnd) {
    const std::array<std::uint8_t, 2> data{0x01, 0x02};
    ByteReader reader(data);

    EXPECT_FALSE(reader.read_bytes(3).has_value());
    EXPECT_EQ(reader.remaining(), 2U);
}

TEST(ByteReader, RejectsHugeByteRangeWithoutOverflow) {
    const std::array<std::uint8_t, 2> data{0x01, 0x02};
    ByteReader reader(data);
    ASSERT_TRUE(reader.read_u8().has_value());

    EXPECT_FALSE(reader.read_bytes(SIZE_MAX).has_value());
    EXPECT_EQ(reader.remaining(), 1U);
}
