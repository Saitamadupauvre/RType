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
#include "engine/network/StringTable.hpp"

namespace engine::app {

class ENGINE_APP_EXPORT SnapshotView {
public:
    SnapshotView(graphics::IRenderer& renderer, std::filesystem::path assets,
                 const core::FieldRegistry& fields);

    void draw(const std::vector<network::EntitySnapshot>& entities,
              const network::StringTable& strings);

    [[nodiscard]] static std::string sprite_of(const std::string& type_name);

private:
    void mirror(const std::vector<network::EntitySnapshot>& entities,
                const network::StringTable& strings);
    void apply(core::Entity entity, const network::EntitySnapshot& snapshot);

    graphics::IRenderer& _renderer;
    const core::FieldRegistry& _fields;
    core::EntityManager _world;
    WorldDraw _draw;
    std::vector<core::Entity> _mirrored;
};

} // namespace engine::app
