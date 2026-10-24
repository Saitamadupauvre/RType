#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "WorldDraw.hpp"
#include "engine/core/Entity.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/graphics/IRenderer.hpp"
#include "engine/network/Client.hpp"
#include "engine/network/Replicated.hpp"

namespace rtype::client {

class SnapshotView {
public:
    SnapshotView(engine::graphics::IRenderer& renderer, std::filesystem::path assets);

    void draw(const std::vector<engine::network::EntitySnapshot>& entities,
              const std::vector<engine::network::ReplicatedType>& types,
              std::uint32_t local_player);

    [[nodiscard]] static std::string sprite_of(const std::string& type_name);

private:
    void mirror(const std::vector<engine::network::EntitySnapshot>& entities,
                const std::vector<engine::network::ReplicatedType>& types);
    void mark_local(const std::vector<engine::network::EntitySnapshot>& entities,
                    std::uint32_t local_player);

    engine::graphics::IRenderer& _renderer;
    engine::core::EntityManager _world;
    WorldDraw _draw;
    std::vector<engine::core::Entity> _mirrored;
};

} // namespace rtype::client
