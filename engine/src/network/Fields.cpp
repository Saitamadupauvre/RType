#include "engine/network/Fields.hpp"

#include "engine/network/Replicated.hpp"

namespace engine::network {

void register_network_fields(core::FieldRegistry& registry) {
    registry.add<Replicated>(
        "Replicated", {core::member_field<&Replicated::owner>(
                          "owner", core::FieldFlags{.replicated = true, .editable = false})});
}

} // namespace engine::network
