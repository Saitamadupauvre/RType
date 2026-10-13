#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ByteReader.hpp"
#include "ByteWriter.hpp"

using engine::network::ByteReader;
using engine::network::ByteWriter;

TEST(ByteWriter, StartsEmpty) {
    const ByteWriter writer;

    EXPECT_EQ(writer.size(), 0U);
    EXPECT_TRUE(writer.bytes().empty());
}

TEST(ByteWriter, WritesUnsignedIntegersInBigEndianOrder) {
    ByteWriter writer;

    writer.write_u8(0xAB);
    writer.write_u16(0x0102);
    writer.write_u32(0x0A0B'0C0D);

    EXPECT_EQ(writer.bytes(),
              (std::vector<std::uint8_t>{0xAB, 0x01, 0x02, 0x0A, 0x0B, 0x0C, 0x0D}));
}

TEST(ByteWriter, WritesSignedIntegersAsTwosComplement) {
    ByteWriter writer;

    writer.write_i8(-1);
    writer.write_i16(-2);
    writer.write_i16(-32768);

    EXPECT_EQ(writer.bytes(), (std::vector<std::uint8_t>{0xFF, 0xFF, 0xFE, 0x80, 0x00}));
}

TEST(ByteWriter, AppendsRawBytes) {
    ByteWriter writer;
    const std::array<std::uint8_t, 3> name{'b', 'y', 'd'};

    writer.write_u8(3);
    writer.write_bytes(name);

    EXPECT_EQ(writer.bytes(), (std::vector<std::uint8_t>{3, 'b', 'y', 'd'}));
}

TEST(ByteWriter, TakeMovesBytesOut) {
    ByteWriter writer;
    writer.write_u16(0xBEEF);

    const auto bytes = std::move(writer).take();

    EXPECT_EQ(bytes, (std::vector<std::uint8_t>{0xBE, 0xEF}));
}

TEST(ByteWriter, RoundTripsThroughByteReader) {
    ByteWriter writer;
    writer.write_u8(200);
    writer.write_i8(-100);
    writer.write_u16(65535);
    writer.write_i16(-12345);
    writer.write_u32(0xDEAD'BEEF);

    ByteReader reader(writer.bytes());

    EXPECT_EQ(reader.read_u8(), 200);
    EXPECT_EQ(reader.read_i8(), -100);
    EXPECT_EQ(reader.read_u16(), 65535);
    EXPECT_EQ(reader.read_i16(), -12345);
    EXPECT_EQ(reader.read_u32(), 0xDEAD'BEEFU);
    EXPECT_TRUE(reader.at_end());
}
