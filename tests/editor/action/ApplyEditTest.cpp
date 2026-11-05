#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "../GameFixture.hpp"
#include "action/ApplyEdit.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/editor/EditorSession.hpp"
#include "engine/script/Instance.hpp"

using editor::apply_edit;
using engine::core::FieldValue;
using engine::core::Vec2;
using engine::editor::EditorSession;
using engine::editor::InstanceId;
using engine::script::FieldOverride;

namespace {

class ApplyEdit : public GameFixture {
protected:
    EditorSession _session = open_level();
    InstanceId _rock = id_of(_session, "rock");
};

} // namespace

TEST_F(ApplyEdit, PlaceAddsAnInstanceOfTheType) {
    const auto before = _session.document().instances.size();

    ASSERT_TRUE(apply_edit(_session, editor::edit::Place{.type = "rock", .position = {7, 8}}).ok);

    ASSERT_EQ(_session.document().instances.size(), before + 1);
    const auto& placed = _session.document().instances.back().instance;
    EXPECT_EQ(placed.type, "rock");
    EXPECT_EQ(placed.position, Vec2(7, 8));
}

TEST_F(ApplyEdit, MoveChangesThePosition) {
    ASSERT_TRUE(apply_edit(_session, editor::edit::Move{.id = _rock, .position = {9, 10}}).ok);

    EXPECT_EQ(instance(_session, _rock).position, Vec2(9, 10));
}

TEST_F(ApplyEdit, RemoveDeletesTheInstance) {
    ASSERT_TRUE(apply_edit(_session, editor::edit::Remove{.id = _rock}).ok);

    EXPECT_EQ(_session.document().find(_rock), nullptr);
}

TEST_F(ApplyEdit, RenameSetsTheLabel) {
    ASSERT_TRUE(apply_edit(_session, editor::edit::Rename{.id = _rock, .name = "boulder"}).ok);

    EXPECT_EQ(instance(_session, _rock).name, "boulder");
}

TEST_F(ApplyEdit, SetFieldStoresAnOverride) {
    const InstanceId ship = id_of(_session, "ship");
    const FieldValue health{std::int64_t{9}};

    ASSERT_TRUE(
        apply_edit(_session,
                   editor::edit::SetField{.id = ship, .key = "Script.health", .value = health})
            .ok);

    EXPECT_EQ(instance(_session, ship).overrides,
              (std::vector<FieldOverride>{{.field = "Script.health", .value = health}}));
}

TEST_F(ApplyEdit, ResetFieldRemovesTheOverride) {
    const InstanceId ship = id_of(_session, "ship");

    ASSERT_TRUE(
        apply_edit(_session, editor::edit::ResetField{.id = ship, .key = "Script.health"}).ok);

    EXPECT_TRUE(instance(_session, ship).overrides.empty());
}

TEST_F(ApplyEdit, ResetAllRemovesEveryOverride) {
    const InstanceId ship = id_of(_session, "ship");

    ASSERT_TRUE(apply_edit(_session, editor::edit::ResetAll{.id = ship}).ok);

    EXPECT_TRUE(instance(_session, ship).overrides.empty());
}

TEST_F(ApplyEdit, UnknownInstanceIsRefused) {
    const InstanceId unknown{.value = 999};

    EXPECT_FALSE(apply_edit(_session, editor::edit::Move{.id = unknown, .position = {}}).ok);
    EXPECT_FALSE(apply_edit(_session, editor::edit::Remove{.id = unknown}).ok);
    EXPECT_FALSE(apply_edit(_session, editor::edit::ResetAll{.id = unknown}).ok);
    EXPECT_FALSE(_session.unsaved());
}
