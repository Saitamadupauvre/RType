#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "WorldDraw.hpp"
#include "engine/core/Entity.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/graphics/IRenderer.hpp"
#include "engine/network/Client.hpp"
#include "engine/network/StringTable.hpp"

namespace rtype::client {

class SnapshotView {
public:
    SnapshotView(engine::graphics::IRenderer& renderer, std::filesystem::path assets,
                 const engine::core::FieldRegistry& fields);

    void draw(const std::vector<engine::network::EntitySnapshot>& entities,
              const engine::network::StringTable& strings, std::uint32_t local_player);

    [[nodiscard]] static std::string sprite_of(const std::string& type_name);

private:
    void mirror(const std::vector<engine::network::EntitySnapshot>& entities,
                const engine::network::StringTable& strings);
    void apply(engine::core::Entity entity, const engine::network::EntitySnapshot& snapshot);
    void mark_local(std::uint32_t local_player);

    engine::graphics::IRenderer& _renderer;
    const engine::core::FieldRegistry& _fields;
    engine::core::EntityManager _world;
    WorldDraw _draw;
    std::vector<engine::core::Entity> _mirrored;
};

} // namespace rtype::client
