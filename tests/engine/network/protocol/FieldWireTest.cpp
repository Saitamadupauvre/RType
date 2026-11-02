#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

#include "TestFields.hpp"
#include "protocol/FieldWire.hpp"

using engine::network::find_wire_field;
using engine::network::to_wire_coordinate;
using engine::network::to_wire_int;
using engine::network::wire_field_name;

TEST(FieldWire, RoundsAndClampsCoordinatesToSigned16Bits) {
    EXPECT_EQ(to_wire_coordinate(1.4F), 1);
    EXPECT_EQ(to_wire_coordinate(1.6F), 2);
    EXPECT_EQ(to_wire_coordinate(-1.5F), -2);
    EXPECT_EQ(to_wire_coordinate(40'000.0F), std::numeric_limits<std::int16_t>::max());
    EXPECT_EQ(to_wire_coordinate(-40'000.0F), std::numeric_limits<std::int16_t>::min());
    EXPECT_EQ(to_wire_coordinate(std::numeric_limits<float>::infinity()),
              std::numeric_limits<std::int16_t>::max());
    EXPECT_EQ(to_wire_coordinate(std::numeric_limits<float>::quiet_NaN()), 0);
}

TEST(FieldWire, ClampsIntsToSigned32Bits) {
    EXPECT_EQ(to_wire_int(-5), -5);
    EXPECT_EQ(to_wire_int(std::int64_t{1'099'511'627'776}),
              std::numeric_limits<std::int32_t>::max());
    EXPECT_EQ(to_wire_int(std::int64_t{-1'099'511'627'776}),
              std::numeric_limits<std::int32_t>::min());
}

TEST(FieldWire, NamesAFieldAfterItsComponent) {
    const auto registry = test_fields::registry();

    const auto field = registry.find_field("Body", "spin");

    EXPECT_EQ(wire_field_name(field), "Body.spin");
    EXPECT_EQ(find_wire_field(registry, "Body.spin").field, field.field);
}

TEST(FieldWire, FindsNothingForMalformedOrUnknownNames) {
    const auto registry = test_fields::registry();

    for (const char* name :
         {"", "Body", "Body.", ".spin", "Body.mass", "Ghost.spin", "Body.spin."}) {
        EXPECT_FALSE(find_wire_field(registry, name)) << name;
    }
}
