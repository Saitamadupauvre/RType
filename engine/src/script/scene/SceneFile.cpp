#include "SceneFile.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "JsonFile.hpp"
#include "engine/core/log/Log.hpp"
#include "entity/Instances.hpp"

namespace engine::script {

namespace {

namespace fs = std::filesystem;
namespace log = engine::core::log;

constexpr std::int64_t scene_version = 1;

const Json* member(const Json& object, std::string_view key) {
    const auto found = object.find(key);
    return found == object.end() ? nullptr : &*found;
}

void check_version(const Json& root, const fs::path& file) {
    const Json* version = member(root, "version");
    if (version == nullptr) {
        throw std::runtime_error(std::format("{} has no version", file.generic_string()));
    }
    if (!version->is_number_integer() || version->get<std::int64_t>() != scene_version) {
        throw std::runtime_error(std::format("{} has unsupported version {} (expected {})",
                                             file.generic_string(), version->dump(),
                                             scene_version));
    }
}

const Json& entries_of(const Json& root, const fs::path& file) {
    if (!root.is_object()) {
        throw std::runtime_error(std::format("{} must hold a JSON object", file.generic_string()));
    }
    check_version(root, file);
    const Json* instances = member(root, "instances");
    if (instances == nullptr || !instances->is_array()) {
        throw std::runtime_error(
            std::format("{} must have an 'instances' array", file.generic_string()));
    }
    return *instances;
}

std::string string_member(const Json& entry, std::string_view key, bool required) {
    const Json* value = member(entry, key);
    if (value == nullptr && !required) {
        return {};
    }
    if (value == nullptr || !value->is_string()) {
        throw std::invalid_argument(std::format("'{}' must be a string", key));
    }
    return value->get<std::string>();
}

core::Vec2 position_of(const Json& entry) {
    const Json* position = member(entry, "position");
    if (position == nullptr) {
        throw std::invalid_argument("'position' is missing");
    }
    try {
        return vec2_from_json(*position);
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument(std::format("'position': {}", error.what()));
    }
}

bool is_entity_type(const RuntimeContext& context, std::string_view type) {
    const Declaration* declaration = context.find_declaration(type);
    return declaration != nullptr && declaration->kind == DeclarationKind::Entity;
}

core::FieldValue override_value(const core::FieldRegistry& fields, const std::string& key,
                                const Json& value) {
    const core::FieldRef field = find_field_key(fields, key);
    if (!field) {
        throw std::invalid_argument(std::format("unknown field '{}'", key));
    }
    if (!field.field->flags().editable) {
        throw std::invalid_argument(std::format("field '{}' is not editable", key));
    }
    return field_from_json(value, field.field->kind(), field.field->element_kind());
}

void read_overrides(const RuntimeContext& context, const Json& entry, std::size_t index,
                    Instance& instance, std::vector<InstanceIssue>& issues) {
    const Json* overrides = member(entry, "overrides");
    if (overrides == nullptr) {
        return;
    }
    if (!overrides->is_object()) {
        throw std::invalid_argument("'overrides' must be an object");
    }
    for (const auto& [key, value] : overrides->items()) {
        try {
            instance.overrides.push_back(
                {.field = key, .value = override_value(context.fields, key, value)});
        } catch (const std::invalid_argument& error) {
            issues.push_back({.instance = index, .field = key, .message = error.what()});
        }
    }
    std::ranges::sort(instance.overrides, {}, &FieldOverride::field);
}

void keep_unknown(const Json& entry, std::size_t index, const std::string& type, SceneLoad& load) {
    load.scene.unknown_instances.push_back(entry.dump());
    load.issues.push_back(
        {.instance = index,
         .field = {},
         .message = std::format("unknown entity type '{}', kept unchanged", type)});
}

void read_entry(const RuntimeContext& context, const Json& entry, std::size_t index,
                SceneLoad& load) {
    if (!entry.is_object()) {
        throw std::invalid_argument(std::format("expected an object, got {}", entry.type_name()));
    }
    std::string type = string_member(entry, "type", true);
    if (!is_entity_type(context, type)) {
        keep_unknown(entry, index, type, load);
        return;
    }
    Instance instance{.name = string_member(entry, "name", false),
                      .type = std::move(type),
                      .position = position_of(entry),
                      .overrides = {}};
    read_overrides(context, entry, index, instance, load.issues);
    load.scene.instances.push_back(std::move(instance));
}

void read_entries(const RuntimeContext& context, const Json& entries, SceneLoad& load) {
    for (std::size_t index = 0; index < entries.size(); ++index) {
        try {
            read_entry(context, entries[index], index, load);
        } catch (const std::invalid_argument& error) {
            load.issues.push_back({.instance = index, .field = {}, .message = error.what()});
        }
    }
}

void log_issue(const fs::path& file, const InstanceIssue& issue) {
    if (issue.field.empty()) {
        log::warn("scene: {}: skipped instance #{}: {}", file.generic_string(), issue.instance,
                  issue.message);
    } else {
        log::warn("scene: {}: instance #{} skipped {}: {}", file.generic_string(), issue.instance,
                  issue.field, issue.message);
    }
}

Json overrides_to_json(std::vector<FieldOverride> overrides) {
    std::ranges::sort(overrides, {}, &FieldOverride::field);
    Json object = Json::object();
    for (const FieldOverride& entry : overrides) {
        object[entry.field] = field_to_json(entry.value);
    }
    return object;
}

Json instance_to_json(const Instance& instance) {
    Json entry = Json::object();
    if (!instance.name.empty()) {
        entry["name"] = instance.name;
    }
    entry["type"] = instance.type;
    entry["position"] = vec2_to_json(instance.position);
    if (!instance.overrides.empty()) {
        entry["overrides"] = overrides_to_json(instance.overrides);
    }
    return entry;
}

Json unknown_to_json(const std::string& text) {
    Json entry = Json::parse(text);
    if (!entry.is_object()) {
        throw std::invalid_argument("an unknown instance must be a JSON object");
    }
    return entry;
}

Json scene_to_json(const Scene& scene) {
    Json instances = Json::array();
    for (const Instance& instance : scene.instances) {
        instances.push_back(instance_to_json(instance));
    }
    for (const std::string& text : scene.unknown_instances) {
        instances.push_back(unknown_to_json(text));
    }
    return Json{{"version", scene_version}, {"instances", std::move(instances)}};
}

} // namespace

SceneLoad read_scene(const RuntimeContext& context, const fs::path& file) {
    SceneLoad load;
    try {
        const Json root = read_json_file(file);
        read_entries(context, entries_of(root, file), load);
    } catch (const std::runtime_error& error) {
        log::error("scene: {}", error.what());
        return {.scene = {}, .issues = {}, .error = error.what()};
    }
    for (const InstanceIssue& issue : load.issues) {
        log_issue(file, issue);
    }
    return load;
}

bool write_scene(const fs::path& file, const Scene& scene) noexcept {
    try {
        return write_json_file(file, scene_to_json(scene));
    } catch (const std::exception& error) {
        log::error("scene: cannot write {}: {}", file.generic_string(), error.what());
    } catch (...) {
        log::error("scene: cannot write {}: unknown error", file.generic_string());
    }
    return false;
}

} // namespace engine::script
