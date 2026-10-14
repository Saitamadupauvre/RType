#include "engine/core/DynamicLibrary.hpp"

#include <string>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace engine::core {

namespace {

#if defined(_WIN32)

std::string last_error() {
    const DWORD code = GetLastError();
    LPSTR buffer = nullptr;
    const DWORD length = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, reinterpret_cast<LPSTR>(&buffer), 0, nullptr);
    std::string message =
        length > 0 ? std::string(buffer, length) : "error " + std::to_string(code);
    LocalFree(buffer);
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
        message.pop_back();
    }
    return message;
}

void* open_library(const std::filesystem::path& path) { return LoadLibraryW(path.c_str()); }

void* find_symbol(void* handle, const std::string& name) {
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name.c_str()));
}

void close_library(void* handle) { FreeLibrary(static_cast<HMODULE>(handle)); }

#else

std::string last_error() {
    const char* message = dlerror();
    return message != nullptr ? message : "unknown error";
}

void* open_library(const std::filesystem::path& path) {
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
}

void* find_symbol(void* handle, const std::string& name) { return dlsym(handle, name.c_str()); }

void close_library(void* handle) { dlclose(handle); }

#endif

} // namespace

DynamicLibrary::DynamicLibrary(const std::filesystem::path& path) : _handle(open_library(path)) {
    if (_handle == nullptr) {
        _error = "cannot open " + path.string() + ": " + last_error();
    }
}

DynamicLibrary::~DynamicLibrary() { close(); }

DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept
    : _handle(std::exchange(other._handle, nullptr)), _error(std::move(other._error)) {}

DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept {
    if (this != &other) {
        close();
        _handle = std::exchange(other._handle, nullptr);
        _error = std::move(other._error);
    }
    return *this;
}

void* DynamicLibrary::raw_symbol(std::string_view name) const {
    if (_handle == nullptr) {
        return nullptr;
    }
    return find_symbol(_handle, std::string(name));
}

void DynamicLibrary::close() noexcept {
    if (_handle != nullptr) {
        close_library(_handle);
        _handle = nullptr;
    }
}

} // namespace engine::core
