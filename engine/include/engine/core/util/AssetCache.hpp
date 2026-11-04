#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "engine/core/Export.hpp"
#include "engine/core/log/Log.hpp"

namespace engine::core {

/** @brief Longest asset name an AssetCache accepts, in bytes. */
inline constexpr std::size_t max_asset_name_length = 255;

/**
 * @brief Tells whether a name safely addresses a file inside an assets folder.
 *
 * @param name Relative path such as `sprites/ship.png`.
 * @return true for 1 to max_asset_name_length bytes made of `/`-separated segments, each one
 * non-empty, not starting with `.`, and made of letters, digits, `_`, `-` and `.`.
 * @note Rejects absolute paths, `..`, backslashes and hidden files. Thread safe.
 */
[[nodiscard]] ENGINE_CORE_EXPORT bool is_valid_asset_name(std::string_view name) noexcept;

/**
 * @brief Loads each asset of a folder once, by name, and unloads them all at the end.
 *
 * The game refers to assets by name; the cache turns a name into a backend handle, loading
 * the file on first use. A rejected name or a failed load yields the placeholder, is logged
 * once and is never retried.
 *
 * @tparam Id Backend handle, compared with `==`; its default value is the placeholder.
 * @note Not thread safe. Use it on the thread that owns the backend, usually the main thread.
 * The backend must outlive the cache.
 */
template <typename Id> class AssetCache {
public:
    /** @brief Loads the file at a path; returns the placeholder on failure. */
    using Load = std::function<Id(const std::filesystem::path&)>;
    /** @brief Frees a handle returned by Load. */
    using Unload = std::function<void(Id)>;

    /**
     * @param folder Folder the names are relative to.
     * @param load Called once per new valid name, with `folder / name`.
     * @param unload Called once per loaded handle by clear() and the destructor.
     */
    AssetCache(std::filesystem::path folder, Load load, Unload unload)
        : _folder(std::move(folder)), _load(std::move(load)), _unload(std::move(unload)) {}

    AssetCache(const AssetCache&) = delete;
    AssetCache& operator=(const AssetCache&) = delete;
    AssetCache(AssetCache&&) = delete;
    AssetCache& operator=(AssetCache&&) = delete;

    ~AssetCache() { clear(); }

    /**
     * @brief Returns the handle of an asset, loading it on first use.
     *
     * @param name Asset name, checked with is_valid_asset_name().
     * @return The loaded handle, or the placeholder for an invalid name or a failed load.
     */
    Id get(const std::string& name) {
        if (const auto found = _ids.find(name); found != _ids.end()) {
            return found->second;
        }
        const Id id = load(name);
        _ids.emplace(name, id);
        return id;
    }

    /** @brief Unloads every loaded asset and forgets every name, failures included. */
    void clear() {
        for (const auto& [name, id] : _ids) {
            if (!(id == Id{})) {
                _unload(id);
            }
        }
        _ids.clear();
    }

    /** @return The number of names known, failures included. */
    [[nodiscard]] std::size_t size() const noexcept { return _ids.size(); }

private:
    Id load(const std::string& name) {
        if (!is_valid_asset_name(name)) {
            log::warn("assets: rejected name '{}'", name);
            return {};
        }
        const Id id = _load(_folder / name);
        if (id == Id{}) {
            log::warn("assets: cannot load '{}'", name);
        }
        return id;
    }

    std::filesystem::path _folder;
    Load _load;
    Unload _unload;
    std::unordered_map<std::string, Id> _ids;
};

} // namespace engine::core
