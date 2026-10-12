#pragma once

#include "engine/core/Export.hpp"

namespace engine::core {

/**
 * @brief Base class for types that must stay at one address: no copy, no move.
 *
 * @note Meant for interfaces and handles. Not polymorphic: derived classes declare their own
 * virtual destructor when needed.
 */
class ENGINE_CORE_EXPORT NonCopyableNonMovable {
public:
    NonCopyableNonMovable(const NonCopyableNonMovable&) = delete;
    NonCopyableNonMovable& operator=(const NonCopyableNonMovable&) = delete;
    NonCopyableNonMovable(NonCopyableNonMovable&&) = delete;
    NonCopyableNonMovable& operator=(NonCopyableNonMovable&&) = delete;

protected:
    NonCopyableNonMovable() = default;
    ~NonCopyableNonMovable() = default;
};

} // namespace engine::core
