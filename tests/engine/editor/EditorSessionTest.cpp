#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "EditorFixture.hpp"
#include "engine/core/ecs/FieldRegistry.hpp"
#include "engine/core/math/Vec2.hpp"
#include "engine/editor/CommandResult.hpp"
#include "engine/editor/EditorSession.hpp"
#include "engine/editor/SceneDocument.hpp"
#include "engine/script/Instance.hpp"

using engine::core::FieldValue;
using engine::core::Rgba;
using engine::core::Vec2;
using engine::editor::CommandResult;
using engine::editor::EditorSession;
using engine::editor::InstanceId;
using engine::script::FieldOverride;

namespace {

using EditorSessionStart = EditorFixture;

class EditorSessionTest : public EditorFixture {
protected:
    InstanceId _hero;
    InstanceId _rock;

    void SetUp() override {
        EditorFixture::SetUp();
        ASSERT_TRUE(session().open_scene("level1").ok);
        ASSERT_EQ(session().document().instances.size(), 2U);
        _hero = session().document().instances[0].id;
        _rock = session().document().instances[1].id;
    }

    [[nodiscard]] const engine::script::Instance& instance(InstanceId id) {
        const auto* entry = session().document().find(id);
        EXPECT_NE(entry, nullptr);
        return entry != nullptr ? entry->instance : _missing;
    }

private:
    engine::script::Instance _missing;
};

const FieldValue health_seven{std::int64_t{7}};

} // namespace

TEST(EditorSession, MissingGameFolderIsRefused) {
    EXPECT_FALSE(EditorSession::open(TEST_FIXTURES_DIR "/editor/missing").has_value());
}

TEST(EditorSession, FolderWithoutScriptsStillOpens) {
    const TempFolder game;
    const auto session = EditorSession::open(game.path());

    ASSERT_TRUE(session.has_value());
    EXPECT_FALSE(session->scripts_loaded());
    EXPECT_TRUE(session->prefabs().empty());
    EXPECT_TRUE(session->scenes().empty());
    EXPECT_FALSE(session->start_scene().has_value());
}

TEST_F(EditorSessionStart, OpensWithNoSceneOpen) {
    EXPECT_TRUE(session().scripts_loaded());
    EXPECT_EQ(session().prefabs(), (std::vector<std::string>{"rock", "ship"}));
    EXPECT_EQ(session().scenes(),
              (std::vector<std::string>{"broken", "faulty", "level1", "level2"}));
    EXPECT_EQ(session().start_scene(), "level1");
    EXPECT_FALSE(session().open_scene_name().has_value());
    EXPECT_TRUE(session().document().instances.empty());
    EXPECT_FALSE(session().unsaved());
}

TEST_F(EditorSessionStart, NoLuaCallbackRunsInTheEditor) {
    ASSERT_TRUE(session().open_scene("level1").ok);
    ASSERT_TRUE(session().place("ship", Vec2{0, 0}).ok);

    EXPECT_FALSE(logged("ran in the editor"));
}

TEST_F(EditorSessionStart, EditsNeedAnOpenScene) {
    EXPECT_FALSE(session().place("rock", Vec2{0, 0}).ok);
    EXPECT_FALSE(session().save().ok);
    EXPECT_FALSE(session().discard_changes().ok);
}

TEST_F(EditorSessionTest, OpenedSceneKeepsUnknownInstances) {
    EXPECT_EQ(session().open_scene_name(), "level1");
    EXPECT_EQ(instance(_hero).name, "hero");
    EXPECT_EQ(instance(_hero).overrides,
              (std::vector<FieldOverride>{{.field = "Script.health", .value = health_seven}}));
    EXPECT_EQ(session().document().unknown_instances.size(), 1U);
    EXPECT_NE(_hero, _rock);
}

TEST_F(EditorSessionTest, EveryInstanceHasAnEditEntity) {
    const auto hero = session().entity_of(_hero);
    const auto rock = session().entity_of(_rock);

    ASSERT_TRUE(hero.has_value());
    ASSERT_TRUE(rock.has_value());
    EXPECT_EQ(session().instance_at(*hero), _hero);
    EXPECT_EQ(session().instance_at(*rock), _rock);
    EXPECT_TRUE(session().problems().empty());
}

TEST_F(EditorSessionTest, PlaceAddsAndSelectsTheInstance) {
    const CommandResult result = session().place("rock", Vec2{9, 8});

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(session().document().instances.size(), 3U);
    const auto& placed = session().document().instances.back();
    EXPECT_EQ(placed.instance.type, "rock");
    EXPECT_EQ(placed.instance.position, (Vec2{9, 8}));
    EXPECT_EQ(session().selection(), placed.id);
    EXPECT_TRUE(session().entity_of(placed.id).has_value());
    EXPECT_TRUE(session().unsaved());
}

TEST_F(EditorSessionTest, PlaceRefusesWhatIsNotADeclaredEntity) {
    EXPECT_FALSE(session().place("ghost", Vec2{0, 0}).ok);
    EXPECT_FALSE(session().place("wave", Vec2{0, 0}).ok);
    EXPECT_FALSE(session().place("rock", Vec2{std::numeric_limits<float>::quiet_NaN(), 0}).ok);
    EXPECT_EQ(session().document().instances.size(), 2U);
    EXPECT_FALSE(session().can_undo());
}

TEST_F(EditorSessionTest, MoveRenameAndRemove) {
    ASSERT_TRUE(session().move(_rock, Vec2{-1, 3}).ok);
    ASSERT_TRUE(session().rename(_rock, "boulder").ok);
    EXPECT_EQ(instance(_rock).position, (Vec2{-1, 3}));
    EXPECT_EQ(instance(_rock).name, "boulder");

    ASSERT_TRUE(session().select(_rock).ok);
    ASSERT_TRUE(session().remove(_rock).ok);
    EXPECT_EQ(session().document().find(_rock), nullptr);
    EXPECT_FALSE(session().selection().has_value());
    EXPECT_FALSE(session().entity_of(_rock).has_value());
}

TEST_F(EditorSessionTest, UnknownIdsAreRefused) {
    const InstanceId unknown{999};

    EXPECT_FALSE(session().move(unknown, Vec2{0, 0}).ok);
    EXPECT_FALSE(session().remove(unknown).ok);
    EXPECT_FALSE(session().rename(unknown, "x").ok);
    EXPECT_FALSE(session().set_field(unknown, "Script.health", health_seven).ok);
    EXPECT_FALSE(session().reset_field(unknown, "Script.health").ok);
    EXPECT_FALSE(session().reset_all(unknown).ok);
    EXPECT_FALSE(session().select(unknown).ok);
    EXPECT_FALSE(session().can_undo());
}

TEST_F(EditorSessionTest, MoveRefusesNonFinitePositions) {
    const float infinity = std::numeric_limits<float>::infinity();

    EXPECT_FALSE(session().move(_rock, Vec2{0, infinity}).ok);
    EXPECT_EQ(instance(_rock).position, (Vec2{5, 6}));
}

TEST_F(EditorSessionTest, SetFieldStoresAnOverride) {
    const FieldValue red{Rgba{.r = 255, .g = 0, .b = 0, .a = 255}};

    ASSERT_TRUE(session().set_field(_hero, "Sprite.color", red).ok);
    ASSERT_TRUE(session().set_field(_hero, "Script.health", FieldValue{std::int64_t{9}}).ok);

    EXPECT_EQ(instance(_hero).overrides,
              (std::vector<FieldOverride>{
                  {.field = "Script.health", .value = FieldValue{std::int64_t{9}}},
                  {.field = "Sprite.color", .value = red}}));
    EXPECT_TRUE(session().problems().empty());
}

TEST_F(EditorSessionTest, SetFieldToTheDefaultRemovesTheOverride) {
    ASSERT_EQ(session().default_value("ship", "Script.health"), FieldValue{std::int64_t{3}});
    ASSERT_EQ(session().default_value("ship", "Sprite.color"),
              (FieldValue{Rgba{.r = 10, .g = 20, .b = 30, .a = 255}}));

    ASSERT_TRUE(session().set_field(_hero, "Script.health", FieldValue{std::int64_t{3}}).ok);
    ASSERT_TRUE(
        session()
            .set_field(_hero, "Sprite.color", FieldValue{Rgba{.r = 10, .g = 20, .b = 30, .a = 255}})
            .ok);

    EXPECT_TRUE(instance(_hero).overrides.empty());
}

TEST_F(EditorSessionTest, SetFieldRefusesInvalidOverrides) {
    EXPECT_FALSE(session().set_field(_rock, "Position.pos", FieldValue{Vec2{}}).ok);
    EXPECT_FALSE(session().set_field(_rock, "Nope.value", FieldValue{1.F}).ok);
    EXPECT_FALSE(session().set_field(_rock, "Script.health", health_seven).ok);
    EXPECT_FALSE(session().set_field(_hero, "Script.health", FieldValue{std::string("a")}).ok);
    EXPECT_FALSE(session().set_field(_rock, "Sprite.color", FieldValue{1.F}).ok);

    const CommandResult refused =
        session().set_field(_rock, "PhysicsBody.type", FieldValue{std::int64_t{5}});
    EXPECT_FALSE(refused.ok);
    EXPECT_NE(refused.message.find("body type"), std::string::npos);

    EXPECT_TRUE(instance(_rock).overrides.empty());
    EXPECT_FALSE(session().can_undo());
    EXPECT_FALSE(session().unsaved());
}

TEST_F(EditorSessionTest, ResetFieldAndResetAll) {
    ASSERT_TRUE(session().set_field(_hero, "Rotation.degrees", FieldValue{45.F}).ok);

    ASSERT_TRUE(session().reset_field(_hero, "Rotation.degrees").ok);
    EXPECT_EQ(instance(_hero).overrides,
              (std::vector<FieldOverride>{{.field = "Script.health", .value = health_seven}}));

    ASSERT_TRUE(session().reset_all(_hero).ok);
    EXPECT_TRUE(instance(_hero).overrides.empty());
}

TEST_F(EditorSessionTest, UndoAndRedoWalkThroughEdits) {
    ASSERT_TRUE(session().place("rock", Vec2{1, 1}).ok);
    const InstanceId placed = session().document().instances.back().id;

    ASSERT_TRUE(session().undo().ok);
    EXPECT_EQ(session().document().find(placed), nullptr);
    EXPECT_FALSE(session().selection().has_value());
    EXPECT_FALSE(session().unsaved());
    EXPECT_TRUE(session().can_redo());

    ASSERT_TRUE(session().redo().ok);
    EXPECT_NE(session().document().find(placed), nullptr);
    EXPECT_TRUE(session().entity_of(placed).has_value());
    EXPECT_FALSE(session().redo().ok);
}

TEST_F(EditorSessionTest, NewEditClearsRedo) {
    ASSERT_TRUE(session().move(_rock, Vec2{1, 1}).ok);
    ASSERT_TRUE(session().undo().ok);
    ASSERT_TRUE(session().move(_rock, Vec2{2, 2}).ok);

    EXPECT_FALSE(session().can_redo());
}

TEST_F(EditorSessionTest, NothingToUndoIsRefused) {
    EXPECT_FALSE(session().undo().ok);
    EXPECT_FALSE(session().redo().ok);
}

TEST_F(EditorSessionTest, EditThatChangesNothingIsNotRecorded) {
    ASSERT_TRUE(session().move(_rock, Vec2{5, 6}).ok);
    ASSERT_TRUE(session().reset_all(_rock).ok);

    EXPECT_FALSE(session().can_undo());
    EXPECT_FALSE(session().unsaved());
}

TEST_F(EditorSessionTest, HistoryKeepsTheLastHundredEdits) {
    for (int step = 1; step <= static_cast<int>(EditorSession::history_limit) + 1; ++step) {
        ASSERT_TRUE(session().move(_rock, Vec2{static_cast<float>(step), 0}).ok);
    }
    for (std::size_t step = 0; step < EditorSession::history_limit; ++step) {
        ASSERT_TRUE(session().undo().ok);
    }

    EXPECT_FALSE(session().undo().ok);
    EXPECT_EQ(instance(_rock).position, (Vec2{1, 0}));
}

TEST_F(EditorSessionTest, SelectionIsNotUndone) {
    ASSERT_TRUE(session().move(_rock, Vec2{1, 1}).ok);
    ASSERT_TRUE(session().select(_hero).ok);
    ASSERT_TRUE(session().undo().ok);

    EXPECT_EQ(session().selection(), _hero);
    ASSERT_TRUE(session().select(std::nullopt).ok);
    EXPECT_FALSE(session().selection().has_value());
}

TEST_F(EditorSessionStart, ProblemsNameTheInstanceAndField) {
    ASSERT_TRUE(session().open_scene("faulty").ok);
    const auto& instances = session().document().instances;
    ASSERT_EQ(instances.size(), 2U);

    ASSERT_EQ(session().problems().size(), 1U);
    EXPECT_EQ(session().problems()[0].instance, instances[1].id);
    EXPECT_EQ(session().problems()[0].field, "PhysicsBody.type");
    EXPECT_TRUE(session().entity_of(instances[1].id).has_value());

    ASSERT_TRUE(session().reset_field(instances[1].id, "PhysicsBody.type").ok);
    EXPECT_TRUE(session().problems().empty());
}

TEST_F(EditorSessionTest, ChangeTypeKeepsWhatTheNewTypeAccepts) {
    ASSERT_TRUE(session().set_field(_hero, "Rotation.degrees", FieldValue{45.F}).ok);

    const CommandResult result = session().change_type(_hero, "rock");

    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_EQ(instance(_hero).type, "rock");
    EXPECT_EQ(instance(_hero).name, "hero");
    EXPECT_EQ(instance(_hero).overrides, (std::vector<FieldOverride>{{.field = "Rotation.degrees",
                                                                      .value = FieldValue{45.F}}}));
    EXPECT_NE(result.message.find("Script.health"), std::string::npos);
    EXPECT_TRUE(session().entity_of(_hero).has_value());
}

TEST_F(EditorSessionTest, ChangeTypeIsUndoable) {
    const auto before = instance(_hero);
    ASSERT_TRUE(session().change_type(_hero, "rock").ok);

    ASSERT_TRUE(session().undo().ok);

    EXPECT_EQ(instance(_hero), before);
}

TEST_F(EditorSessionTest, ChangeTypeToTheSameTypeChangesNothing) {
    ASSERT_TRUE(session().change_type(_hero, "ship").ok);

    EXPECT_FALSE(session().can_undo());
    EXPECT_FALSE(session().unsaved());
}

TEST_F(EditorSessionTest, ChangeTypeRefusesWhatIsNotADeclaredEntity) {
    EXPECT_FALSE(session().change_type(_hero, "ghost").ok);
    EXPECT_FALSE(session().change_type(_hero, "").ok);
    EXPECT_FALSE(session().change_type(InstanceId{}, "rock").ok);

    EXPECT_EQ(instance(_hero).type, "ship");
    EXPECT_FALSE(session().can_undo());
}

TEST_F(EditorSessionTest, DuplicateCopiesTheInstanceRightAfterIt) {
    ASSERT_TRUE(session().duplicate(_hero, Vec2{16, 0}).ok);

    const auto& instances = session().document().instances;
    ASSERT_EQ(instances.size(), 3U);
    EXPECT_EQ(instances[0].id, _hero);
    EXPECT_EQ(instances[2].id, _rock);
    const engine::script::Instance& copy = instances[1].instance;
    EXPECT_EQ(copy.type, "ship");
    EXPECT_EQ(copy.name, "hero");
    EXPECT_EQ(copy.position, (Vec2{17, 2}));
    EXPECT_EQ(copy.overrides, instance(_hero).overrides);
}

TEST_F(EditorSessionTest, DuplicateSelectsTheCopy) {
    ASSERT_TRUE(session().duplicate(_hero, Vec2{0, 16}).ok);

    const InstanceId copy = session().document().instances[1].id;
    EXPECT_NE(copy, _hero);
    EXPECT_EQ(session().selection(), copy);
    EXPECT_TRUE(session().entity_of(copy).has_value());
}

TEST_F(EditorSessionTest, DuplicateIsOneUndoableEdit) {
    ASSERT_TRUE(session().duplicate(_hero, Vec2{16, 16}).ok);

    ASSERT_TRUE(session().undo().ok);

    EXPECT_EQ(session().document().instances.size(), 2U);
    EXPECT_FALSE(session().selection().has_value());
    EXPECT_FALSE(session().unsaved());
}

TEST_F(EditorSessionTest, DuplicateRefusesAnUnknownIdOrANonFiniteOffset) {
    EXPECT_FALSE(session().duplicate(InstanceId{}, Vec2{16, 0}).ok);
    EXPECT_FALSE(session().duplicate(_hero, Vec2{std::numeric_limits<float>::infinity(), 0}).ok);
    EXPECT_FALSE(session().duplicate(_hero, Vec2{0, std::numeric_limits<float>::quiet_NaN()}).ok);

    EXPECT_EQ(session().document().instances.size(), 2U);
    EXPECT_FALSE(session().can_undo());
}

TEST_F(EditorSessionTest, ReloadScriptsSeesANewScript) {
    std::ofstream(_game.path() / "scripts" / "crate.lua") << "entity \"crate\"\n";

    const CommandResult result = session().reload_scripts();

    ASSERT_TRUE(result.ok);
    EXPECT_NE(result.message.find("3 prefabs"), std::string::npos) << result.message;
    EXPECT_EQ(session().prefabs(), (std::vector<std::string>{"crate", "rock", "ship"}));
    EXPECT_TRUE(session().place("crate", Vec2{0, 0}).ok);
}

TEST_F(EditorSessionTest, ReloadScriptsKeepsTheSceneHistoryAndSelection) {
    ASSERT_TRUE(session().move(_rock, Vec2{40, 40}).ok);
    ASSERT_TRUE(session().select(_hero).ok);

    ASSERT_TRUE(session().reload_scripts().ok);

    EXPECT_EQ(session().open_scene_name(), "level1");
    EXPECT_EQ(instance(_rock).position, (Vec2{40, 40}));
    EXPECT_TRUE(session().unsaved());
    EXPECT_TRUE(session().can_undo());
    EXPECT_EQ(session().selection(), _hero);
    EXPECT_TRUE(session().entity_of(_hero).has_value());
}

TEST_F(EditorSessionTest, ReloadScriptsTurnsADeletedTypeIntoAProblem) {
    std::filesystem::remove(_game.path() / "scripts" / "rock.lua");

    ASSERT_TRUE(session().reload_scripts().ok);

    EXPECT_EQ(session().prefabs(), (std::vector<std::string>{"ship"}));
    EXPECT_FALSE(session().entity_of(_rock).has_value());
    EXPECT_FALSE(session().problems().empty());
}

TEST_F(EditorSessionTest, ReloadScriptsSaysWhenAScriptFails) {
    std::ofstream(_game.path() / "scripts" / "broken.lua") << "entity \"broken\" {\n";

    const CommandResult result = session().reload_scripts();

    ASSERT_TRUE(result.ok);
    EXPECT_FALSE(session().scripts_loaded());
    EXPECT_NE(result.message.find("failed"), std::string::npos) << result.message;
}
