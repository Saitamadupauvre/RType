#pragma once

#include "engine/core/FieldRegistry.hpp"
#include "engine/network/Export.hpp"

namespace engine::network {

/**
 * @brief Registers the fields of Replicated: `owner`, an int sent in every snapshot.
 *
 * Replicated's `type_id` is not a field: every snapshot entity carries it in its header.
 *
 * @note Setting `owner` to a value outside 0..255 throws core::FieldValueOutOfRange.
 * @throws core::DuplicateComponentName If Replicated is already registered.
 */
ENGINE_NETWORK_EXPORT void register_network_fields(core::FieldRegistry& registry);

} // namespace engine::network
