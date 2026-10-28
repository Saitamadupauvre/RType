#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "engine/core/event/EventBus.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/network/Events.hpp"
#include "engine/network/StringTable.hpp"

#include "event/GameEvents.hpp"
#include "protocol/Messages.hpp"

namespace {

namespace logger = engine::core::log;
using engine::core::Vec2;
using engine::core::event::EventBus;
using engine::network::EventPayload;
using engine::network::NamedEvent;
using engine::network::NamedEventState;
using engine::network::PlayerLeft;
using engine::network::PlayerLeftEvent;
using engine::network::publish_received;
using engine::network::SoundEvent;
using engine::network::SoundPlayed;
using engine::network::StringCategory;
using engine::network::StringTable;
using engine::network::to_payload;

constexpr std::uint16_t explosion_id = 3;
constexpr std::uint16_t at_key = 1;
constexpr std::uint16_t size_key = 2;

StringTable event_strings() {
    StringTable table;
    EXPECT_TRUE(
        table.insert({.category = StringCategory::event, .id = explosion_id, .name = "explosion"}));
    EXPECT_TRUE(table.insert({.category = StringCategory::event_key, .id = at_key, .name = "at"}));
    EXPECT_TRUE(
        table.insert({.category = StringCategory::event_key, .id = size_key, .name = "size"}));
    EXPECT_TRUE(table.insert({.category = StringCategory::sound, .id = 4, .name = "unrelated"}));
    return table;
}

NamedEvent explosion() {
    return {
        .name = "explosion",
        .arguments = {{.key = "at", .value = Vec2{10.0F, -3.0F}}, {.key = "size", .value = 2.5F}}};
}

NamedEventState explosion_state() {
    return {.name_id = explosion_id,
            .arguments = {{.key_id = at_key, .value = Vec2{10.0F, -3.0F}},
                          {.key_id = size_key, .value = 2.5F}}};
}

class GameEventsTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};

    [[nodiscard]] bool logged(const std::string& part) const {
        return std::ranges::any_of(_sink->records(), [&](const logger::MemoryRecord& record) {
            return record.level == logger::Level::Warn &&
                   record.message.find(part) != std::string::npos;
        });
    }
};

} // namespace

TEST(GameEvents, AudioEventsKeepTheirIds) {
    EXPECT_EQ((to_payload(SoundPlayed{.source_entity_id = 1, .sound_id = 5})),
              (EventPayload{SoundEvent{.source_entity_id = 1, .sound_id = 5}}));
    EXPECT_EQ(to_payload(engine::network::MusicChanged{.music_id = 3}),
              (EventPayload{engine::network::MusicEvent{.music_id = 3}}));
}

TEST(GameEvents, PublishesEachReceivedEventAsItsGameEvent) {
    EventBus bus;
    std::vector<std::uint16_t> sounds;
    std::vector<std::uint32_t> left;
    std::vector<std::uint16_t> musics;
    bus.subscribe<SoundPlayed>([&](const SoundPlayed& event) { sounds.push_back(event.sound_id); });
    bus.subscribe<PlayerLeft>([&](const PlayerLeft& event) { left.push_back(event.player_id); });
    bus.subscribe<engine::network::MusicChanged>(
        [&](const engine::network::MusicChanged& event) { musics.push_back(event.music_id); });

    publish_received(bus, SoundEvent{.source_entity_id = 1, .sound_id = 8}, StringTable{});
    publish_received(bus, PlayerLeftEvent{.player_entity_id = 2}, StringTable{});
    publish_received(bus, engine::network::MusicEvent{.music_id = 9}, StringTable{});

    EXPECT_EQ(sounds, (std::vector<std::uint16_t>{8}));
    EXPECT_EQ(left, (std::vector<std::uint32_t>{2}));
    EXPECT_EQ(musics, (std::vector<std::uint16_t>{9}));
}

TEST_F(GameEventsTest, NamedEventIsSentWithTheIdsOfItsNames) {
    EXPECT_EQ(to_payload(explosion(), event_strings()), EventPayload{explosion_state()});
}

TEST_F(GameEventsTest, NamedEventWithoutArgumentsIsSent) {
    EXPECT_EQ(to_payload(NamedEvent{.name = "explosion", .arguments = {}}, event_strings()),
              (EventPayload{NamedEventState{.name_id = explosion_id, .arguments = {}}}));
}

TEST_F(GameEventsTest, DropsANamedEventMissingFromTheStringTable) {
    EXPECT_FALSE(to_payload(NamedEvent{.name = "unrelated", .arguments = {}}, event_strings()));
    EXPECT_TRUE(logged("dropped event 'unrelated': not in the string table"));
}

TEST_F(GameEventsTest, DropsANamedEventWithAKeyMissingFromTheStringTable) {
    auto event = explosion();
    event.arguments.push_back({.key = "colour", .value = true});

    EXPECT_FALSE(to_payload(event, event_strings()));
    EXPECT_TRUE(logged("dropped event 'explosion': key 'colour' is not in the string table"));
}

TEST_F(GameEventsTest, DropsANamedEventBreakingTheWireLimits) {
    auto repeated = explosion();
    repeated.arguments.push_back({.key = "at", .value = Vec2{}});
    auto long_text = explosion();
    long_text.arguments[1].value = std::string(engine::network::max_event_text_length + 1, 'a');

    EXPECT_FALSE(to_payload(repeated, event_strings()));
    EXPECT_FALSE(to_payload(long_text, event_strings()));
    EXPECT_TRUE(logged("dropped event 'explosion': more than 16 arguments"));
}

TEST_F(GameEventsTest, PublishesAReceivedNamedEventWithItsNames) {
    EventBus bus;
    std::vector<NamedEvent> received;
    bus.subscribe<NamedEvent>([&](const NamedEvent& event) { received.push_back(event); });

    publish_received(bus, explosion_state(), event_strings());

    ASSERT_EQ(received.size(), 1U);
    EXPECT_EQ(received.front(), explosion());
    ASSERT_NE(received.front().find("size"), nullptr);
    EXPECT_EQ(*received.front().find("size"), engine::core::FieldValue{2.5F});
    EXPECT_EQ(received.front().find("colour"), nullptr);
}

TEST_F(GameEventsTest, DropsAReceivedNamedEventWithAnUnknownNameOrKey) {
    EventBus bus;
    std::vector<NamedEvent> received;
    bus.subscribe<NamedEvent>([&](const NamedEvent& event) { received.push_back(event); });
    auto unknown_key = explosion_state();
    unknown_key.arguments.push_back({.key_id = 9, .value = true});

    publish_received(bus, NamedEventState{.name_id = 4, .arguments = {}}, event_strings());
    publish_received(bus, unknown_key, event_strings());

    EXPECT_TRUE(received.empty());
    EXPECT_TRUE(logged("dropped event with unknown name id 4"));
    EXPECT_TRUE(logged("dropped event 'explosion' with unknown key id 9"));
}
