#include "action/InspectorModel.hpp"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace editor {

namespace {

using engine::core::FieldValue;
using engine::editor::EditorSession;
using engine::script::Instance;

constexpr std::string_view position_key = "Position.pos";
constexpr std::string_view script_prefix = "Script.";

const FieldValue* override_of(const Instance& instance, std::string_view key) {
    const auto found = std::ranges::find(instance.overrides, key,
                                         &engine::script::FieldOverride::field);
    return found == instance.overrides.end() ? nullptr : &found->value;
}

void add_field(std::vector<InspectorField>& fields, EditorSession& session,
               const Instance& instance, std::string key) {
    if (const FieldValue* value = override_of(instance, key)) {
        fields.push_back({.key = std::move(key), .kind = engine::core::kind_of(*value),
                          .value = *value, .overridden = true});
        return;
    }
    if (std::optional<FieldValue> value = session.default_value(instance.type, key)) {
        fields.push_back({.key = std::move(key), .kind = engine::core::kind_of(*value),
                          .value = std::move(*value), .overridden = false});
    }
}

std::vector<InspectorField> registry_fields(EditorSession& session, const Instance& instance,
                                            engine::core::Entity entity) {
    std::vector<InspectorField> fields;
    const auto& registry = session.runtime().fields();
    for (const engine::core::FieldRef ref : registry.fields_of(session.world(), entity)) {
        if (!ref.field->flags().editable) {
            continue;
        }
        std::string key = ref.component->name() + "." + ref.field->name();
        if (key == position_key) {
            continue;
        }
        add_field(fields, session, instance, std::move(key));
    }
    return fields;
}

std::vector<InspectorField> script_fields(EditorSession& session, const Instance& instance) {
    std::vector<InspectorField> fields;
    for (const auto& declared : session.runtime().script_fields(instance.type)) {
        add_field(fields, session, instance, std::string(script_prefix) + declared.name);
    }
    return fields;
}

std::filesystem::path script_of(const EditorSession& session, std::string_view type) {
    const auto declaration = session.runtime().find_declaration(type);
    if (!declaration) {
        return {};
    }
    return session.game() / "scripts" / declaration->source;
}

} // namespace

std::optional<InspectorModel> inspect_selection(EditorSession& session) {
    const std::optional<engine::editor::InstanceId> id = session.selection();
    const auto* entry = id ? session.document().find(*id) : nullptr;
    const std::optional<engine::core::Entity> entity = id ? session.entity_of(*id) : std::nullopt;
    if (entry == nullptr || !entity) {
        return std::nullopt;
    }
    return InspectorModel{.id = *id,
                          .fields = registry_fields(session, entry->instance, *entity),
                          .script_fields = script_fields(session, entry->instance),
                          .script = script_of(session, entry->instance.type)};
}

} // namespace editor
