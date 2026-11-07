#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#include "../../graphics/fixtures/NullRenderer.hpp"
#include "engine/core/metrics/Metrics.hpp"
#include "engine/core/time/ManualClock.hpp"
#include "render/MetricsOverlay.hpp"

using namespace std::chrono_literals;
using engine::app::draw_metrics_overlay;
using engine::app::metrics_overlay_lines;
using engine::core::Vec2;
using engine::core::metrics::Metrics;
using engine::core::time::ManualClock;
using engine::graphics::Color;
using engine::graphics::Rect;

namespace {

struct DrawnRect {
    Rect rect;
    Color color;
    bool filled;
};

struct DrawnLine {
    Vec2 from;
    Vec2 to;
    Color color;
};

struct DrawnText {
    std::string text;
    Vec2 position;
};

class RecordingRenderer final : public fixture::NullRenderer {
public:
    void draw_rect(const Rect& rect, Color color, bool filled) override {
        rects.push_back({.rect = rect, .color = color, .filled = filled});
    }
    void draw_line(Vec2 from, Vec2 to, float, Color color) override {
        lines.push_back({.from = from, .to = to, .color = color});
    }
    void draw_text(const std::string& text, Vec2 position, float, Color) override {
        texts.push_back({.text = text, .position = position});
    }
    void draw_sprite(const engine::graphics::SpriteDraw&) override { ++sprites; }
    void begin_world(const engine::core::CameraView&) override { ++world_passes; }

    std::vector<DrawnRect> rects;
    std::vector<DrawnLine> lines;
    std::vector<DrawnText> texts;
    int sprites{0};
    int world_passes{0};
};

void play_client_frame(Metrics& metrics, double frame_ms, double snapshots) {
    metrics.record_time("frame", std::chrono::duration_cast<engine::core::time::Duration>(
                                     std::chrono::duration<double, std::milli>(frame_ms)));
    metrics.set_gauge("fps", 1000.0 / frame_ms);
    metrics.record_time("lua.update", 2ms);
    metrics.record_time("physics", 500us);
    metrics.record_time("draw", 4ms);
    metrics.set_gauge("entities", 42);
    metrics.set_gauge("process.cpu", 37.4);
    metrics.set_gauge("process.memory", 85.25);
    metrics.set_gauge("network.rtt", 12.4);
    metrics.set_gauge("network.packets_lost", 3);
    metrics.set_gauge("network.bytes_in", 3.0 * 1024);
    metrics.set_gauge("network.bytes_out", 1024);
    metrics.count("network.snapshots", snapshots);
    metrics.end_frame();
}

void play_server_tick(Metrics& metrics) {
    metrics.record_time("tick", 1ms);
    metrics.record_time("lua.update", 250us);
    metrics.record_time("physics", 100us);
    metrics.set_gauge("entities", 50);
    metrics.end_frame();
}

bool contains(const std::vector<std::string>& lines, const std::string& text) {
    return std::ranges::any_of(
        lines, [&text](const std::string& line) { return line.find(text) != std::string::npos; });
}

} // namespace

TEST(MetricsOverlay, ListsEveryClientValue) {
    const ManualClock clock;
    Metrics client(clock);
    play_client_frame(client, 20.0, 1);
    play_client_frame(client, 20.0, 1);

    const auto lines = metrics_overlay_lines({.client = client});

    EXPECT_TRUE(contains(lines, "FPS 50")) << lines.front();
    EXPECT_TRUE(contains(lines, "frame 20.00 ms"));
    EXPECT_TRUE(contains(lines, "Lua 2.00 ms"));
    EXPECT_TRUE(contains(lines, "physics 0.50 ms"));
    EXPECT_TRUE(contains(lines, "draw 4.00 ms"));
    EXPECT_TRUE(contains(lines, "Entities 42"));
    EXPECT_TRUE(contains(lines, "CPU 37 %"));
    EXPECT_TRUE(contains(lines, "memory 85.2 MiB") || contains(lines, "memory 85.3 MiB"));
    EXPECT_TRUE(contains(lines, "Ping 12 ms"));
    EXPECT_TRUE(contains(lines, "lost 3"));
    EXPECT_TRUE(contains(lines, "snapshots 50/s"));
    EXPECT_TRUE(contains(lines, "In 3.0 KiB/s"));
    EXPECT_TRUE(contains(lines, "out 1.0 KiB/s"));
    EXPECT_FALSE(contains(lines, "Server"));
}

TEST(MetricsOverlay, AddsTheServerTimesWhenGiven) {
    const ManualClock clock;
    Metrics client(clock);
    Metrics server(clock);
    play_client_frame(client, 16.0, 1);
    play_server_tick(server);

    const auto lines = metrics_overlay_lines({.client = client, .server = &server});

    EXPECT_TRUE(contains(lines, "Server tick 1.00 ms  Lua 0.25 ms  physics 0.10 ms"));
    EXPECT_TRUE(contains(lines, "Entities 42  server 50"));
}

TEST(MetricsOverlay, ShowsADashForValuesNotMeasuredYet) {
    const ManualClock clock;
    const Metrics client(clock);

    const auto lines = metrics_overlay_lines({.client = client});

    EXPECT_TRUE(contains(lines, "FPS -"));
    EXPECT_TRUE(contains(lines, "Ping - ms"));
    EXPECT_TRUE(contains(lines, "snapshots -/s"));
    EXPECT_TRUE(contains(lines, "In - KiB/s"));
}

TEST(MetricsOverlay, SnapshotsPerSecondFollowTheFrameTime) {
    const ManualClock clock;
    Metrics client(clock);
    for (int frame = 0; frame < 10; ++frame) {
        play_client_frame(client, 10.0, frame % 2);
    }

    EXPECT_TRUE(contains(metrics_overlay_lines({.client = client}), "snapshots 50/s"));
}

TEST(MetricsOverlay, DrawsEveryLineInScreenSpaceOverAPanel) {
    const ManualClock clock;
    Metrics client(clock);
    play_client_frame(client, 16.0, 1);
    RecordingRenderer renderer;

    draw_metrics_overlay(renderer, {.client = client});

    const auto lines = metrics_overlay_lines({.client = client});
    ASSERT_EQ(renderer.texts.size(), lines.size());
    ASSERT_FALSE(renderer.rects.empty());
    const DrawnRect& panel = renderer.rects.front();
    EXPECT_TRUE(panel.filled);
    EXPECT_LT(panel.color.a, 255);
    for (std::size_t index = 0; index < lines.size(); ++index) {
        EXPECT_EQ(renderer.texts[index].text, lines[index]);
        EXPECT_GT(renderer.texts[index].position.y, panel.rect.y);
        EXPECT_LT(renderer.texts[index].position.y, panel.rect.y + panel.rect.height);
    }
    for (std::size_t index = 1; index < renderer.texts.size(); ++index) {
        EXPECT_GT(renderer.texts[index].position.y, renderer.texts[index - 1].position.y);
    }
    EXPECT_EQ(renderer.sprites, 0);
    EXPECT_EQ(renderer.world_passes, 0);
}

TEST(MetricsOverlay, GraphHasOneBarPerFrameScaledToItsFrameTime) {
    const ManualClock clock;
    Metrics client(clock, 4);
    play_client_frame(client, 10.0, 1);
    play_client_frame(client, 25.0, 1);
    play_client_frame(client, 60.0, 1);
    RecordingRenderer renderer;

    draw_metrics_overlay(renderer, {.client = client});

    ASSERT_EQ(renderer.rects.size(), 2U);
    const Rect graph = renderer.rects[1].rect;
    EXPECT_FALSE(renderer.rects[1].filled);
    EXPECT_GE(graph.y, renderer.texts.back().position.y);
    ASSERT_EQ(renderer.lines.size(), 4U);
    const float bottom = graph.y + graph.height;
    const auto height_of = [bottom](const DrawnLine& line) { return bottom - line.to.y; };
    EXPECT_FLOAT_EQ(height_of(renderer.lines[0]), graph.height * 10.F / 50.F);
    EXPECT_FLOAT_EQ(height_of(renderer.lines[1]), graph.height * 25.F / 50.F);
    EXPECT_FLOAT_EQ(height_of(renderer.lines[2]), graph.height);
    EXPECT_LT(renderer.lines[0].from.x, renderer.lines[1].from.x);
    EXPECT_GT(renderer.lines[0].color.g, renderer.lines[0].color.r);
    EXPECT_GT(renderer.lines[2].color.r, renderer.lines[2].color.g);
    for (const auto& bar : {renderer.lines[0], renderer.lines[1], renderer.lines[2]}) {
        EXPECT_GE(bar.from.x, graph.x);
        EXPECT_LE(bar.from.x, graph.x + graph.width);
        EXPECT_FLOAT_EQ(bar.from.y, bottom);
    }
    const DrawnLine& target = renderer.lines[3];
    EXPECT_FLOAT_EQ(target.from.y, target.to.y);
    EXPECT_NEAR(bottom - target.from.y, graph.height * (1000.F / 60.F) / 50.F, 1e-3);
}
