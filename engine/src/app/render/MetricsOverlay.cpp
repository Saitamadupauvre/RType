#include "render/MetricsOverlay.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <numeric>
#include <optional>
#include <string_view>

namespace engine::app {

namespace {

using core::metrics::Metrics;

constexpr float panel_margin = 8.F;
constexpr float panel_padding = 8.F;
constexpr float panel_width = 380.F;
constexpr float text_size = 16.F;
constexpr float line_height = 20.F;
constexpr float graph_height = 60.F;
constexpr float graph_gap = 6.F;
constexpr double graph_scale_ms = 50.0;
constexpr double target_frame_ms = 1000.0 / 60.0;
constexpr double slow_frame_ms = 1000.0 / 30.0;
constexpr double bytes_per_kibibyte = 1024.0;
constexpr double milliseconds_per_second = 1000.0;

constexpr graphics::Color panel_color{.r = 0, .g = 0, .b = 0, .a = 170};
constexpr graphics::Color text_color{.r = 255, .g = 255, .b = 255, .a = 255};
constexpr graphics::Color frame_color{.r = 255, .g = 255, .b = 255, .a = 255};
constexpr graphics::Color target_color{.r = 80, .g = 220, .b = 80, .a = 255};
constexpr graphics::Color fast_color{.r = 80, .g = 220, .b = 80, .a = 255};
constexpr graphics::Color slow_color{.r = 240, .g = 200, .b = 60, .a = 255};
constexpr graphics::Color late_color{.r = 240, .g = 70, .b = 70, .a = 255};

std::optional<double> last_of(const Metrics& metrics, std::string_view name) {
    const auto summary = metrics.summary(name);
    if (!summary || summary->samples == 0) {
        return std::nullopt;
    }
    return summary->last;
}

std::optional<double> average_of(const Metrics& metrics, std::string_view name) {
    const auto summary = metrics.summary(name);
    if (!summary || summary->samples == 0) {
        return std::nullopt;
    }
    return summary->average;
}

std::optional<double> max_of(const Metrics& metrics, std::string_view name) {
    const auto summary = metrics.summary(name);
    if (!summary || summary->samples == 0) {
        return std::nullopt;
    }
    return summary->max;
}

std::string decimal(std::optional<double> value, int precision) {
    if (!value) {
        return "-";
    }
    return std::format("{:.{}f}", *value, precision);
}

std::string milliseconds(std::optional<double> value) { return decimal(value, 2) + " ms"; }

std::string kibibytes(std::optional<double> value) {
    if (!value) {
        return "- KiB/s";
    }
    return decimal(*value / bytes_per_kibibyte, 1) + " KiB/s";
}

std::optional<double> snapshots_per_second(const Metrics& metrics) {
    const auto snapshots = metrics.history("network.snapshots");
    const auto frames = metrics.history("frame");
    const std::size_t count = std::min(snapshots.size(), frames.size());
    if (count == 0) {
        return std::nullopt;
    }
    const double received =
        std::accumulate(snapshots.end() - static_cast<std::ptrdiff_t>(count), snapshots.end(), 0.0);
    const double elapsed_ms =
        std::accumulate(frames.end() - static_cast<std::ptrdiff_t>(count), frames.end(), 0.0);
    if (elapsed_ms <= 0.0) {
        return std::nullopt;
    }
    return received * milliseconds_per_second / elapsed_ms;
}

std::string frame_line(const Metrics& client) {
    return std::format("FPS {}  frame {}  max {}", decimal(average_of(client, "fps"), 0),
                       milliseconds(average_of(client, "frame")),
                       milliseconds(max_of(client, "frame")));
}

std::string client_times_line(const Metrics& client) {
    return std::format(
        "Lua {}  physics {}  draw {}", milliseconds(average_of(client, "lua.update")),
        milliseconds(average_of(client, "physics")), milliseconds(average_of(client, "draw")));
}

std::string server_times_line(const Metrics& server) {
    return std::format("Server tick {}  Lua {}  physics {}",
                       milliseconds(average_of(server, "tick")),
                       milliseconds(average_of(server, "lua.update")),
                       milliseconds(average_of(server, "physics")));
}

std::string entities_line(const MetricsSources& sources) {
    std::string line = "Entities " + decimal(last_of(sources.client, "entities"), 0);
    if (sources.server != nullptr) {
        line += "  server " + decimal(last_of(*sources.server, "entities"), 0);
    }
    return line;
}

std::string process_line(const Metrics& client) {
    return std::format("CPU {} %  memory {} MiB", decimal(last_of(client, "process.cpu"), 0),
                       decimal(last_of(client, "process.memory"), 1));
}

std::string latency_line(const Metrics& client) {
    return std::format("Ping {} ms  lost {}  snapshots {}/s",
                       decimal(last_of(client, "network.rtt"), 0),
                       decimal(last_of(client, "network.packets_lost"), 0),
                       decimal(snapshots_per_second(client), 0));
}

std::string traffic_line(const Metrics& client) {
    return std::format("In {}  out {}", kibibytes(last_of(client, "network.bytes_in")),
                       kibibytes(last_of(client, "network.bytes_out")));
}

graphics::Color bar_color(double frame_ms) {
    if (frame_ms <= target_frame_ms) {
        return fast_color;
    }
    return frame_ms <= slow_frame_ms ? slow_color : late_color;
}

float graph_y(const graphics::Rect& graph, double frame_ms) {
    const double share = std::clamp(frame_ms / graph_scale_ms, 0.0, 1.0);
    return graph.y + graph.height - static_cast<float>(share) * graph.height;
}

void draw_frame_graph(graphics::IRenderer& renderer, const graphics::Rect& graph,
                      const Metrics& client) {
    renderer.draw_rect(graph, frame_color, false);
    const auto frames = client.history("frame");
    const float bar_width = graph.width / static_cast<float>(client.capacity());
    const float bottom = graph.y + graph.height;
    for (std::size_t index = 0; index < frames.size(); ++index) {
        const float x = graph.x + ((static_cast<float>(index) + 0.5F) * bar_width);
        renderer.draw_line({x, bottom}, {x, graph_y(graph, frames[index])}, bar_width,
                           bar_color(frames[index]));
    }
    const float target = graph_y(graph, target_frame_ms);
    renderer.draw_line({graph.x, target}, {graph.x + graph.width, target}, 1.F, target_color);
}

} // namespace

std::vector<std::string> metrics_overlay_lines(const MetricsSources& sources) {
    std::vector<std::string> lines{frame_line(sources.client), client_times_line(sources.client)};
    if (sources.server != nullptr) {
        lines.push_back(server_times_line(*sources.server));
    }
    lines.push_back(entities_line(sources));
    lines.push_back(process_line(sources.client));
    lines.push_back(latency_line(sources.client));
    lines.push_back(traffic_line(sources.client));
    return lines;
}

void draw_metrics_overlay(graphics::IRenderer& renderer, const MetricsSources& sources) {
    const auto lines = metrics_overlay_lines(sources);
    const float text_height = static_cast<float>(lines.size()) * line_height;
    const graphics::Rect panel{.x = panel_margin,
                               .y = panel_margin,
                               .width = panel_width,
                               .height =
                                   (panel_padding * 2.F) + text_height + graph_gap + graph_height};
    renderer.draw_rect(panel, panel_color);
    float y = panel.y + panel_padding;
    for (const auto& line : lines) {
        renderer.draw_text(line, {panel.x + panel_padding, y}, text_size, text_color);
        y += line_height;
    }
    const graphics::Rect graph{.x = panel.x + panel_padding,
                               .y = y + graph_gap,
                               .width = panel.width - (panel_padding * 2.F),
                               .height = graph_height};
    draw_frame_graph(renderer, graph, sources.client);
}

} // namespace engine::app
