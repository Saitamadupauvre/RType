#pragma once

#include "engine/core/Export.hpp"
#include <cstdint>
#include <string_view>
#include <typeinfo>

namespace engine::core {

using ComponentTypeId = std::uint32_t;

ENGINE_CORE_EXPORT ComponentTypeId component_type_id(std::string_view type_name);

template <typename T> ComponentTypeId component_type_id() {
    return component_type_id(typeid(T).name());
}

} // namespace engine::core
