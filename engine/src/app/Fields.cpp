#include "Fields.hpp"

#include "engine/network/Fields.hpp"
#include "engine/physics/Fields.hpp"

namespace engine::app {

core::FieldRegistry replicated_fields() {
    core::FieldRegistry fields;
    core::register_core_fields(fields);
    physics::register_physics_fields(fields);
    network::register_network_fields(fields);
    return fields;
}

} // namespace engine::app
