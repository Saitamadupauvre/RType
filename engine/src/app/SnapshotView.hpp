#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "WorldDraw.hpp"
#include "engine/app/Export.hpp"
#include "engine/core/Entity.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/graphics/IRenderer.hpp"
#include "engine/network/Client.hpp"

namespace engine::app {

class ENGINE_APP_EXPORT SnapshotView {
public:
    SnapshotView(graphics::IRenderer& renderer, const std::filesystem::path& assets,
                 const core::FieldRegistry& fields);

    void draw(const std::vector<network::EntitySnapshot>& entities);
    bool preload_texture(const std::string& texture);

private:
    void mirror(const std::vector<network::EntitySnapshot>& entities);
    void apply(core::Entity entity, const network::EntitySnapshot& snapshot);

    graphics::IRenderer& _renderer;
    const core::FieldRegistry& _fields;
    core::EntityManager _world;
    WorldDraw _draw;
    std::vector<core::Entity> _mirrored;
};

} // namespace engine::app
