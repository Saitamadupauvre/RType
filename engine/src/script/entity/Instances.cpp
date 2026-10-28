#include "Instances.hpp"

#include <algorithm>
#include <exception>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/log/Log.hpp"

namespace engine::script {

namespace {

namespace log = engine::core::log;

std::string field_key(core::FieldRef field) {
    return std::format("{}.{}", field.component->name(), field.field->name());
}

core::FieldRef find_field_key(const core::FieldRegistry& fields, std::string_view key) {
    const auto separator = key.find('.');
    if (separator == std::string_view::npos) {
        return {};
    }
    return fields.find_field(key.substr(0, separator), key.substr(separator + 1));
}

class Prototypes {
public:
    explicit Prototypes(RuntimeContext& context) : _context(context) {}

    std::optional<core::FieldValue> default_of(const Declaration& declaration,
                                               core::FieldRef field) {
        const auto entity = prototype_of(declaration);
        if (!entity) {
            return std::nullopt;
        }
        return _context.fields.get(_scratch, *entity, field);
    }

private:
    std::optional<core::Entity> prototype_of(const Declaration& declaration) {
        const auto found = _entities.find(declaration.name);
        if (found != _entities.end()) {
            return found->second;
        }
        std::optional<core::Entity> entity;
        try {
            entity = _context.entities.prototype(_scratch, declaration);
        } catch (const std::exception& error) {
            log::warn("instances: no defaults for '{}': {}", declaration.name, error.what());
        }
        _entities.emplace(declaration.name, entity);
        return entity;
    }

    RuntimeContext& _context;
    core::EntityManager _scratch;
    std::map<std::string, std::optional<core::Entity>, std::less<>> _entities;
};

const Declaration* declaration_of(const RuntimeContext& context, core::Entity entity) {
    const auto type = context.entities.type_of(entity);
    return type ? context.find_declaration(*type) : nullptr;
}

Instance capture_instance(RuntimeContext& context, Prototypes& prototypes, core::Entity entity,
                          const Declaration& declaration) {
    core::EntityManager& world = *context.entities.world();
    const core::FieldRef position = context.fields.find_field("Position", "pos");
    Instance instance{.name = context.entities.name_of(entity).value_or(""),
                      .type = declaration.name,
                      .position = world.get<core::Position>(entity).pos,
                      .overrides = {}};
    for (const core::FieldRef field : context.fields.fields_of(world, entity)) {
        if (!field.field->flags().editable || field.field == position.field) {
            continue;
        }
        auto value = context.fields.get(world, entity, field);
        if (value && value != prototypes.default_of(declaration, field)) {
            instance.overrides.push_back({.field = field_key(field), .value = std::move(*value)});
        }
    }
    std::ranges::sort(instance.overrides, {}, &FieldOverride::field);
    return instance;
}

std::optional<std::string> override_error(const core::FieldRegistry& fields,
                                          core::EntityManager& world, core::Entity entity,
                                          const FieldOverride& field_override) {
    const core::FieldRef field = find_field_key(fields, field_override.field);
    if (!field) {
        return std::format("unknown field '{}'", field_override.field);
    }
    if (!field.field->flags().editable) {
        return std::format("field '{}' is not editable", field_override.field);
    }
    try {
        fields.set(world, entity, field, field_override.value);
    } catch (const std::exception& error) {
        return error.what();
    }
    return std::nullopt;
}

void spawn_instance(RuntimeContext& context, const Instance& instance, std::size_t index,
                    std::vector<InstanceIssue>& issues) {
    const Declaration* declaration = context.find_declaration(instance.type);
    if (declaration == nullptr || declaration->kind != DeclarationKind::Entity) {
        issues.push_back({.instance = index,
                          .field = {},
                          .message = std::format("unknown entity type '{}'", instance.type)});
        return;
    }
    const auto entity = context.entities.spawn(*declaration, instance.position);
    if (!entity) {
        issues.push_back({.instance = index,
                          .field = {},
                          .message = std::format("cannot spawn '{}'", instance.type)});
        return;
    }
    context.entities.rename(*entity, instance.name);
    context.entities.finish_spawn(*entity);
    for (const FieldOverride& field_override : instance.overrides) {
        auto error =
            override_error(context.fields, *context.entities.world(), *entity, field_override);
        if (error) {
            issues.push_back(
                {.instance = index, .field = field_override.field, .message = std::move(*error)});
        }
    }
}

void log_issue(const Instance& instance, const InstanceIssue& issue) {
    if (issue.field.empty()) {
        log::warn("instances: skipped #{} '{}': {}", issue.instance, instance.type, issue.message);
    } else {
        log::warn("instances: #{} '{}' skipped {}: {}", issue.instance, instance.type, issue.field,
                  issue.message);
    }
}

} // namespace

std::vector<Instance> capture_instances(RuntimeContext& context) {
    std::vector<Instance> instances;
    if (context.entities.world() == nullptr) {
        return instances;
    }
    Prototypes prototypes(context);
    for (const core::Entity entity : context.entities.live_entities()) {
        const Declaration* declaration = declaration_of(context, entity);
        if (declaration != nullptr && context.entities.is_active(entity)) {
            instances.push_back(capture_instance(context, prototypes, entity, *declaration));
        }
    }
    return instances;
}

std::vector<InstanceIssue> spawn_instances(RuntimeContext& context,
                                           std::span<const Instance> instances) {
    std::vector<InstanceIssue> issues;
    for (std::size_t index = 0; index < instances.size(); ++index) {
        const std::size_t first_issue = issues.size();
        try {
            spawn_instance(context, instances[index], index, issues);
        } catch (const std::exception& error) {
            issues.push_back({.instance = index, .field = {}, .message = error.what()});
        }
        for (std::size_t issue = first_issue; issue < issues.size(); ++issue) {
            log_issue(instances[index], issues[issue]);
        }
    }
    return issues;
}

} // namespace engine::script
