#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "engine/core/EntityManager.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/script/EventCommand.hpp"
#include "engine/script/ScriptRuntime.hpp"
#include "engine/script/ServerApi.hpp"

namespace logger = engine::core::log;
using engine::core::AssetName;
using engine::core::EntityManager;
using engine::core::Rgba;
using engine::core::Vec2;
using engine::script::EventArgument;
using engine::script::EventCommand;
using engine::script::ScriptRuntime;
using engine::script::ServerApi;

namespace {

std::filesystem::path events_fixture(const std::string& name) {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "events" / name;
}

class EventsTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    EntityManager _world;
    ScriptRuntime _runtime = ScriptRuntime::builder().with<ServerApi>().build();
    ServerApi& _server = _runtime.api<ServerApi>();

    void SetUp() override { _runtime.attach_world(_world); }

    void load_declared() { ASSERT_TRUE(_runtime.load_scripts(events_fixture("declared"))); }

    void expect_emit_error(const std::string& code, const std::string& expected) {
        EXPECT_FALSE(_runtime.run_string(code, "events")) << code;
        EXPECT_EQ(count_logged(logger::Level::Error, expected), 1U) << code;
        EXPECT_TRUE(_server.take_event_commands().empty()) << code;
    }

    [[nodiscard]] std::size_t count_logged(logger::Level level, const std::string& part) const {
        return static_cast<std::size_t>(
            std::ranges::count_if(_sink->records(), [&](const logger::MemoryRecord& record) {
                return record.level == level && record.message.find(part) != std::string::npos;
            }));
    }
};

} // namespace

TEST_F(EventsTest, ListsDeclaredNamesAndKeysInAscendingOrder) {
    load_declared();

    EXPECT_EQ(_runtime.event_names(), (std::vector<std::string>{"boss.died", "explosion", "ping"}));
    EXPECT_EQ(_runtime.event_keys(), (std::vector<std::string>{"at", "final", "label", "points",
                                                               "size", "sprite", "tint"}));
}

TEST_F(EventsTest, NothingIsDeclaredWithoutScripts) {
    EXPECT_TRUE(_runtime.event_names().empty());
    EXPECT_TRUE(_runtime.event_keys().empty());
    EXPECT_TRUE(_server.take_event_commands().empty());
}

TEST_F(EventsTest, EmitQueuesArgumentsInKeyOrder) {
    load_declared();

    ASSERT_TRUE(_runtime.run_string(
        R"(emit("explosion", { size = 1.5, label = "big", at = { x = 3, y = 4 } }))", "events"));

    EXPECT_EQ(
        _server.take_event_commands(),
        (std::vector<EventCommand>{{.name = "explosion",
                                    .arguments = {{.key = "at", .value = Vec2{3.0F, 4.0F}},
                                                  {.key = "label", .value = std::string("big")},
                                                  {.key = "size", .value = 1.5F}}}}));
}

TEST_F(EventsTest, EmitConvertsEveryKind) {
    load_declared();

    ASSERT_TRUE(_runtime.run_string(R"(emit("boss.died", { points = 500, final = true,
        tint = { r = 255, g = 10, b = 0 }, sprite = "sprites/boss.png" }))",
                                    "events"));

    EXPECT_EQ(
        _server.take_event_commands(),
        (std::vector<EventCommand>{
            {.name = "boss.died",
             .arguments = {{.key = "final", .value = true},
                           {.key = "points", .value = std::int64_t{500}},
                           {.key = "sprite", .value = AssetName{"sprites/boss.png"}},
                           {.key = "tint", .value = Rgba{.r = 255, .g = 10, .b = 0, .a = 255}}}}}));
}

TEST_F(EventsTest, LeftOutArgumentsAreAbsent) {
    load_declared();

    ASSERT_TRUE(
        _runtime.run_string(R"(emit("ping") emit("explosion", {}) emit("ping", nil))", "events"));

    EXPECT_EQ(_server.take_event_commands(),
              (std::vector<EventCommand>{{.name = "ping", .arguments = {}},
                                         {.name = "explosion", .arguments = {}},
                                         {.name = "ping", .arguments = {}}}));
}

TEST_F(EventsTest, EntityCallbacksCanEmit) {
    load_declared();
    const auto ship = _runtime.spawn("ship");
    ASSERT_TRUE(ship.has_value());

    ASSERT_TRUE(_runtime.call(*ship, "blow_up"));

    EXPECT_EQ(_server.take_event_commands(),
              (std::vector<EventCommand>{{.name = "explosion",
                                          .arguments = {{.key = "at", .value = Vec2{1.0F, -2.0F}},
                                                        {.key = "size", .value = 2.0F}}}}));
}

TEST_F(EventsTest, TakingCommandsEmptiesTheQueue) {
    load_declared();
    ASSERT_TRUE(_runtime.run_string(R"(emit("ping"))", "events"));

    EXPECT_EQ(_server.take_event_commands().size(), 1U);
    EXPECT_TRUE(_server.take_event_commands().empty());
}

TEST_F(EventsTest, EmitRejectsUnknownEvents) {
    load_declared();

    expect_emit_error(R"(emit("missing"))", "emit: unknown event 'missing', declare it with event");
}

TEST_F(EventsTest, EmitRejectsANameThatIsNotAString) {
    load_declared();

    expect_emit_error("emit(42)", "emit: name must be a string");
}

TEST_F(EventsTest, EmitRejectsArgumentsThatAreNotATable) {
    load_declared();

    expect_emit_error(R"(emit("ping", 3))",
                      "emit: arguments of event 'ping' must be a table or nil");
}

TEST_F(EventsTest, EmitRejectsUndeclaredKeys) {
    load_declared();

    expect_emit_error(R"(emit("explosion", { colour = 1 }))",
                      "emit: event 'explosion' has no argument 'colour'");
}

TEST_F(EventsTest, EmitRejectsNonStringKeys) {
    load_declared();

    expect_emit_error(R"(emit("explosion", { 1, 2 }))",
                      "emit: argument keys of event 'explosion' must be strings");
}

TEST_F(EventsTest, EmitRejectsValuesOfTheWrongKind) {
    load_declared();

    expect_emit_error(R"(emit("explosion", { size = "big" }))",
                      "event 'explosion' argument 'size' cannot be set to a string");
}

TEST_F(EventsTest, EmitRejectsNonFiniteFloatsAndFractionalInts) {
    load_declared();

    expect_emit_error(R"(emit("explosion", { size = math.huge }))",
                      "event 'explosion' argument 'size' must be a finite number");
    expect_emit_error(R"(emit("boss.died", { points = 1.5 }))",
                      "event 'boss.died' argument 'points' must be a whole number");
}

TEST_F(EventsTest, EmitRejectsTextLongerThanTheLimit) {
    load_declared();

    ASSERT_TRUE(_runtime.run_string(R"(emit("explosion", { label = string.rep("a", )" +
                                        std::to_string(engine::script::max_event_text_length) +
                                        R"() }))",
                                    "events"));
    EXPECT_EQ(_server.take_event_commands().size(), 1U);
    expect_emit_error(R"(emit("explosion", { label = string.rep("a", )" +
                          std::to_string(engine::script::max_event_text_length + 1) + R"() }))",
                      "emit: event 'explosion' argument 'label' is longer than 63 bytes");
}

TEST_F(EventsTest, DropsEventsBeyondTheLimitAndWarnsOnce) {
    load_declared();

    ASSERT_TRUE(_runtime.run_string(R"(for _ = 1, 100 do emit("ping") end)", "events"));

    EXPECT_EQ(_server.take_event_commands().size(), 64U);
    EXPECT_EQ(count_logged(logger::Level::Warn, "more than 64 events emitted"), 1U);
}

TEST_F(EventsTest, DeclaringAfterLoadingFails) {
    load_declared();

    EXPECT_FALSE(_runtime.run_string(R"(event "late" {})", "events"));

    EXPECT_EQ(_runtime.event_names(), (std::vector<std::string>{"boss.died", "explosion", "ping"}));
    EXPECT_EQ(count_logged(logger::Level::Error,
                           "event 'late' can only be declared while the scripts load"),
              1U);
}

TEST_F(EventsTest, RejectsADuplicateDeclaration) {
    EXPECT_FALSE(_runtime.load_scripts(events_fixture("duplicate")));

    EXPECT_EQ(_runtime.event_keys(), (std::vector<std::string>{"at"}));
    EXPECT_EQ(count_logged(logger::Level::Error, "event 'explosion' is already declared"), 1U);
}

TEST_F(EventsTest, RejectsAnInvalidName) {
    EXPECT_FALSE(_runtime.load_scripts(events_fixture("invalid_name")));

    EXPECT_TRUE(_runtime.event_names().empty());
    EXPECT_EQ(count_logged(logger::Level::Error, "'../boom' is not a valid event name"), 1U);
}

TEST_F(EventsTest, RejectsAKeyThatIsNotAnIdentifier) {
    EXPECT_FALSE(_runtime.load_scripts(events_fixture("invalid_key")));

    EXPECT_TRUE(_runtime.event_names().empty());
    EXPECT_EQ(count_logged(logger::Level::Error, "argument keys must be identifiers"), 1U);
}

TEST_F(EventsTest, RejectsUnknownAndListKinds) {
    for (const std::string fixture : {"unknown_kind", "list_kind"}) {
        ScriptRuntime runtime = ScriptRuntime::builder().with<ServerApi>().build();

        EXPECT_FALSE(runtime.load_scripts(events_fixture(fixture))) << fixture;
        EXPECT_TRUE(runtime.event_names().empty()) << fixture;
    }
    EXPECT_EQ(count_logged(logger::Level::Error,
                           "must be one of float, int, bool, string, vec2, color, asset"),
              2U);
}

TEST_F(EventsTest, RejectsMoreArgumentsThanTheLimit) {
    EXPECT_FALSE(_runtime.load_scripts(events_fixture("too_many")));

    EXPECT_TRUE(_runtime.event_names().empty());
    EXPECT_EQ(count_logged(logger::Level::Error, "event 'crowded' has more than 16 arguments"), 1U);
}

TEST_F(EventsTest, RejectsASchemaThatIsNotATable) {
    EXPECT_FALSE(_runtime.load_scripts(events_fixture("not_a_table")));

    EXPECT_EQ(
        count_logged(logger::Level::Error, "event 'explosion' expects a table of argument kinds"),
        1U);
}
