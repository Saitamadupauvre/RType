#pragma once

#include <string>

#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/network/Fields.hpp"

namespace test_fields {

struct Body {
    engine::core::Vec2 pos;
    float spin = 0.F;
};

struct Label {
    std::string text;
    bool hidden = false;
};

inline engine::core::FieldRegistry registry() {
    using engine::core::field_flags::editable;
    using engine::core::field_flags::replicated;
    engine::core::FieldRegistry fields;
    fields.add<Body>("Body", {engine::core::member_field<&Body::pos>("pos", replicated),
                              engine::core::member_field<&Body::spin>("spin", replicated)});
    fields.add<Label>("Label", {engine::core::member_field<&Label::text>("text", replicated),
                                engine::core::member_field<&Label::hidden>("hidden", editable)});
    engine::network::register_network_fields(fields);
    return fields;
}

inline void register_components(engine::core::EntityManager& world) {
    world.register_component<Body>();
    world.register_component<Label>();
}

} // namespace test_fields
