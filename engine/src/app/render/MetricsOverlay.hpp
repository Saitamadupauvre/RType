#pragma once

#include <string>
#include <vector>

#include "engine/app/Export.hpp"
#include "engine/core/metrics/Metrics.hpp"
#include "engine/graphics/IRenderer.hpp"

namespace engine::app {

struct MetricsSources {
    const core::metrics::Metrics& client;
    const core::metrics::Metrics* server{nullptr};
};

ENGINE_APP_EXPORT std::vector<std::string> metrics_overlay_lines(const MetricsSources& sources);

ENGINE_APP_EXPORT void draw_metrics_overlay(graphics::IRenderer& renderer,
                                            const MetricsSources& sources);

} // namespace engine::app
