#include <gtest/gtest.h>

#include "engine/core/FieldRegistry.hpp"
#include "engine/network/Fields.hpp"

TEST(NetworkFields, OwnerIsReplicatedButNotEditable) {
    engine::core::FieldRegistry fields;
    engine::network::register_network_fields(fields);
    const engine::core::FieldRef owner = fields.find_field("Replicated", "owner");
    ASSERT_TRUE(owner);
    EXPECT_TRUE(owner.field->flags().replicated);
    EXPECT_FALSE(owner.field->flags().editable);
}
