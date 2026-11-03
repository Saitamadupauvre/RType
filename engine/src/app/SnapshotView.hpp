#pragma once

#include <vector>

#include "engine/app/Export.hpp"
#include "engine/core/Entity.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/network/Client.hpp"

namespace engine::app {

class ENGINE_APP_EXPORT SnapshotView {
public:
    explicit SnapshotView(const core::FieldRegistry& fields);

    core::EntityManager& mirror(const std::vector<network::EntitySnapshot>& entities);

private:
    void apply(core::Entity entity, const network::EntitySnapshot& snapshot);

    const core::FieldRegistry& _fields;
    core::EntityManager _world;
    std::vector<core::Entity> _mirrored;
};

} // namespace engine::app
