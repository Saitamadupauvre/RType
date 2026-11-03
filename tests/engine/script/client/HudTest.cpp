#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "engine/core/Components.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/script/ClientApi.hpp"
#include "engine/script/HudCommand.hpp"
#include "engine/script/ScriptRuntime.hpp"

namespace logger = engine::core::log;
using engine::core::Rgba;
using engine::core::Vec2;
using engine::script::ClientApi;
using engine::script::HudCommand;
using engine::script::HudRect;
using engine::script::HudSprite;
using engine::script::HudText;
using engine::script::max_hud_commands;
using engine::script::ScriptRuntime;

namespace {

class HudTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    ScriptRuntime _runtime = ScriptRuntime::builder().with<ClientApi>().build();
    ClientApi& _client = _runtime.api<ClientApi>();

    void SetUp() override {
        ASSERT_TRUE(
            _runtime.load_scripts(std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "hud"));
    }

    std::vector<HudCommand> draw(const std::string& body) {
        EXPECT_TRUE(_runtime.run_string("function hud() " + body + " end", "setup"));
        return _client.draw_hud();
    }

    [[nodiscard]] bool logged(logger::Level level, const std::string& part) const {
        return std::ranges::any_of(_sink->records(), [&](const auto& record) {
            return record.level == level && record.message.find(part) != std::string::npos;
        });
    }
};

} // namespace

TEST_F(HudTest, WithoutDrawHudNothingIsDrawn) {
    ScriptRuntime runtime = ScriptRuntime::builder().with<ClientApi>().build();

    EXPECT_TRUE(runtime.api<ClientApi>().draw_hud().empty());
}

TEST_F(HudTest, CommandsAreReturnedInCallOrder) {
    const auto commands = draw(R"(
        draw.rect(10, 20, 300, 40, { r = 0, g = 0, b = 0, a = 128 })
        draw.text("SCORE 12", 30, 28, 24, { r = 255, g = 200, b = 0 })
        draw.sprite("ui/heart.png", 1800, 20)
    )");

    ASSERT_EQ(commands.size(), 3U);
    EXPECT_EQ(std::get<HudRect>(commands[0]), (HudRect{.position = {10, 20},
                                                       .size = {300, 40},
                                                       .color = {.r = 0, .g = 0, .b = 0, .a = 128},
                                                       .filled = true}));
    EXPECT_EQ(std::get<HudText>(commands[1]),
              (HudText{.text = "SCORE 12",
                       .position = {30, 28},
                       .size = 24,
                       .color = {.r = 255, .g = 200, .b = 0, .a = 255}}));
    EXPECT_EQ(std::get<HudSprite>(commands[2]), (HudSprite{.texture = "ui/heart.png",
                                                           .position = {1800, 20},
                                                           .scale = {1, 1},
                                                           .rotation = 0,
                                                           .color = {}}));
}

TEST_F(HudTest, OptionalArgumentsOverrideTheDefaults) {
    const auto commands = draw(R"(
        draw.rect(0, 0, 5, 5, nil, false)
        draw.sprite("ui/heart.png", 1, 2, { scale = { x = -2, y = 3 }, rotation = 90,
                                            color = { r = 1, g = 2, b = 3 } })
        draw.sprite("ui/heart.png", 1, 2, { scale = 4 })
    )");

    ASSERT_EQ(commands.size(), 3U);
    EXPECT_FALSE(std::get<HudRect>(commands[0]).filled);
    EXPECT_EQ(std::get<HudRect>(commands[0]).color, Rgba{});
    const auto& turned = std::get<HudSprite>(commands[1]);
    EXPECT_EQ(turned.scale, (Vec2{-2, 3}));
    EXPECT_FLOAT_EQ(turned.rotation, 90.F);
    EXPECT_EQ(turned.color, (Rgba{.r = 1, .g = 2, .b = 3, .a = 255}));
    EXPECT_EQ(std::get<HudSprite>(commands[2]).scale, (Vec2{4, 4}));
}

TEST_F(HudTest, EachFrameStartsFromAnEmptyHud) {
    draw("draw.rect(0, 0, 1, 1)");

    const auto commands = _client.draw_hud();

    EXPECT_EQ(commands.size(), 1U);
}

TEST_F(HudTest, DrawingOutsideDrawHudRaises) {
    EXPECT_FALSE(_runtime.run_string("draw.rect(0, 0, 1, 1)", "outside"));

    EXPECT_TRUE(logged(logger::Level::Error, "draw.rect: only callable from draw_hud()"));
    EXPECT_TRUE(_client.draw_hud().empty());
}

TEST_F(HudTest, AnErrorKeepsTheCommandsQueuedBeforeIt) {
    const auto commands = draw(R"(
        draw.rect(0, 0, 1, 1)
        error("boom")
        draw.rect(5, 5, 1, 1)
    )");

    ASSERT_EQ(commands.size(), 1U);
    EXPECT_EQ(std::get<HudRect>(commands[0]).position, (Vec2{0, 0}));
    EXPECT_TRUE(logged(logger::Level::Error, "boom"));
}

TEST_F(HudTest, InvalidArgumentsRaiseAndQueueNothing) {
    const std::vector<std::pair<std::string, std::string>> cases{
        {"draw.text(12, 0, 0, 10)", "draw.text: text must be a string"},
        {"draw.text('a', 0/0, 0, 10)", "draw.text: x must be a finite number"},
        {"draw.text('a', 0, 0, 0)", "draw.text: size must be a positive number"},
        {"draw.text('a', 0, 0, 10, 'red')", "draw.text 'color' cannot be set to a string"},
        {"draw.text(string.rep('a', 257), 0, 0, 10)", "draw.text: text must be at most 256 bytes"},
        {"draw.rect(0, 0, -1, 1)", "draw.rect: w must not be negative"},
        {"draw.rect(0, 0, 1)", "draw.rect: h must be a finite number"},
        {"draw.rect(0, 0, 1, 1, nil, 'yes')", "draw.rect: filled must be a boolean"},
        {"draw.rect(0, 0, 1, 1, { r = 300, g = 0, b = 0 })", "'r' must be in [0, 255]"},
        {"draw.sprite('ui/ghost.png', 0, 0)",
         "draw.sprite: unknown texture 'ui/ghost.png', declare it with textures"},
        {"draw.sprite('ui/heart.png', 0, math.huge)", "draw.sprite: y must be a finite number"},
        {"draw.sprite('ui/heart.png', 0, 0, 3)", "draw.sprite: options must be a table"},
        {"draw.sprite('ui/heart.png', 0, 0, { scale = 'big' })",
         "draw.sprite: scale must be a number or a { x, y } table"},
        {"draw.sprite('ui/heart.png', 0, 0, { scale = { x = 1 } })",
         "draw.sprite: scale.y must be a finite number"},
    };
    for (const auto& [code, message] : cases) {
        EXPECT_TRUE(draw(code).empty()) << code;
        EXPECT_TRUE(logged(logger::Level::Error, message)) << code;
    }
}

TEST_F(HudTest, CommandsBeyondTheLimitAreDroppedAndLoggedOnce) {
    const auto commands = draw("for i = 1, 1100 do draw.rect(i, 0, 1, 1) end");

    EXPECT_EQ(commands.size(), max_hud_commands);
    EXPECT_EQ(std::ranges::count_if(_sink->records(),
                                    [](const auto& record) {
                                        return record.level == logger::Level::Warn &&
                                               record.message.find("commands in one frame") !=
                                                   std::string::npos;
                                    }),
              1);
}

TEST_F(HudTest, DrawExistsOnlyInTheClientSet) {
    ScriptRuntime server = ScriptRuntime::builder().build();

    EXPECT_TRUE(server.run_string("assert(draw == nil)", "core"));
}
