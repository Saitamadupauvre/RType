#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "engine/core/Entity.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/network/Export.hpp"
#include "engine/network/StringTable.hpp"

#include "protocol/Messages.hpp"

namespace engine::network {

struct ReplicatedField {
    std::uint16_t id{};
    core::FieldRef field;
};

class ENGINE_NETWORK_EXPORT ReplicatedFields {
public:
    ReplicatedFields(const core::FieldRegistry& registry, StringTable& strings);

    [[nodiscard]] const std::string& error() const noexcept { return _error; }
    [[nodiscard]] const std::vector<ReplicatedField>& fields() const noexcept { return _fields; }
    [[nodiscard]] std::vector<FieldState> read(core::EntityManager& world,
                                               core::Entity entity) const;

private:
    void declare(core::FieldRef field, StringTable& strings);
    void check_entity_size();
    void fail(std::string error);

    std::reference_wrapper<const core::FieldRegistry> _registry;
    std::vector<ReplicatedField> _fields;
    std::string _error;
};

} // namespace engine::network
