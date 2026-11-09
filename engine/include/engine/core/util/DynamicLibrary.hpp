#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <type_traits>

#include "engine/core/Export.hpp"

namespace engine::core {

/**
 * @brief Shared library opened at runtime (dlopen / LoadLibrary), closed on destruction.
 *
 * Opening never throws: a library that failed to open converts to false and error() says why.
 *
 * @note Move-only. Not thread safe. Pointers obtained with symbol() are invalid once the library
 * is closed.
 */
class DynamicLibrary {
public:
    /**
     * @brief Opens a shared library.
     *
     * @param path File to open, used as is (no search path, no added extension).
     */
    ENGINE_CORE_EXPORT explicit DynamicLibrary(const std::filesystem::path& path);
    ENGINE_CORE_EXPORT ~DynamicLibrary();

    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;
    ENGINE_CORE_EXPORT DynamicLibrary(DynamicLibrary&& other) noexcept;
    ENGINE_CORE_EXPORT DynamicLibrary& operator=(DynamicLibrary&& other) noexcept;

    /** @return true if the library is open. */
    [[nodiscard]] explicit operator bool() const noexcept { return _handle != nullptr; }

    /** @return Why opening failed, or an empty string if the library is open. */
    [[nodiscard]] const std::string& error() const noexcept { return _error; }

    /**
     * @brief Looks up an exported function.
     *
     * @tparam FunctionPointer Pointer to the function type, e.g. `int (*)()`.
     * @param name Exported, unmangled name of the function.
     * @return The function, or nullptr if the library is closed or does not export it.
     */
    template <typename FunctionPointer>
        requires std::is_function_v<std::remove_pointer_t<FunctionPointer>>
    [[nodiscard]] FunctionPointer symbol(std::string_view name) const {
        return reinterpret_cast<FunctionPointer>(raw_symbol(name));
    }

private:
    [[nodiscard]] ENGINE_CORE_EXPORT void* raw_symbol(std::string_view name) const;
    void close() noexcept;

    void* _handle{nullptr};
    std::string _error;
};

} // namespace engine::core
