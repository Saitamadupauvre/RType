#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

#include "ServerApp.hpp"
#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/SystemScheduler.hpp"
#include "engine/core/time/ManualClock.hpp"
#include "engine/network/Replicated.hpp"

using engine::core::EntityManager;
using engine::core::SystemPhase;
using engine::core::time::Duration;
using engine::core::time::ManualClock;
using rtype::server::ServerApp;
using rtype::server::ServerConfig;

namespace {

constexpr Duration tick_step = std::chrono::nanoseconds(1'000'000'000 / 60);

} // namespace

TEST(ServerApp, RegistersTheComponentsSentInSnapshots) {
    const ManualClock clock;
    ServerApp app(clock, ServerConfig{.port = 4242, .max_players = 4});

    EXPECT_TRUE(app.registry().is_registered<engine::network::Replicated>());
    EXPECT_TRUE(app.registry().is_registered<engine::core::Position>());
    EXPECT_TRUE(app.registry().is_registered<engine::core::Velocity>());
}

TEST(ServerApp, TickRunsSystemsWithTheFixedStep) {
    const ManualClock clock;
    ServerApp app(clock, ServerConfig{.port = 4242, .max_players = 4});
    std::vector<float> steps;
    app.systems().add("record", SystemPhase::Input,
                      [&steps](EntityManager&, float dt) { steps.push_back(dt); });

    app.tick(tick_step);

    ASSERT_EQ(steps.size(), 1U);
    EXPECT_FLOAT_EQ(steps.front(), 1.0F / 60.0F);
}

TEST(ServerApp, RunTicksAtFixedRateUntilAskedToStop) {
    ManualClock clock;
    ServerApp app(clock, ServerConfig{.port = 4242, .max_players = 4});
    int ticks = 0;
    app.systems().add("count", SystemPhase::Input, [&ticks](EntityManager&, float) { ++ticks; });
    int checks = 0;

    app.run([&] {
        clock.advance(tick_step * 2);
        return ++checks > 3;
    });

    EXPECT_EQ(ticks, 6);
}

TEST(ServerApp, StartReportsPortAlreadyInUse) {
    const ManualClock clock;
    ServerApp first(clock, ServerConfig{.port = 0, .max_players = 4});
    std::string error;
    ASSERT_TRUE(first.start(error)) << error;
    ServerApp second(clock, ServerConfig{.port = first.network().port(), .max_players = 4});

    EXPECT_FALSE(second.start(error));
    EXPECT_NE(error.find(std::to_string(first.network().port())), std::string::npos) << error;
}
