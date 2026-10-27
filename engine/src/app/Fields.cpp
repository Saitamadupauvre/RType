#include "GameFields.hpp"

#include "engine/network/Fields.hpp"
#include "engine/physics/Fields.hpp"

namespace rtype::common {

engine::core::FieldRegistry game_fields() {
    engine::core::FieldRegistry fields;
    engine::core::register_core_fields(fields);
    engine::physics::register_physics_fields(fields);
    engine::network::register_network_fields(fields);
    return fields;
}

} // namespace rtype::common
