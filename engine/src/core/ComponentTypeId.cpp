#include "engine/core/ComponentTypeId.hpp"

#include <mutex>
#include <string>
#include <unordered_map>

namespace engine::core {

namespace {

struct TypeIdTable {
    std::mutex mutex;
    std::unordered_map<std::string, ComponentTypeId> ids;
};

TypeIdTable& table() {
    static TypeIdTable instance;
    return instance;
}

} // namespace

ComponentTypeId component_type_id(std::string_view type_name) {
    TypeIdTable& ids = table();
    const std::scoped_lock lock(ids.mutex);
    const auto next_id = static_cast<ComponentTypeId>(ids.ids.size());
    return ids.ids.try_emplace(std::string(type_name), next_id).first->second;
}

} // namespace engine::core
