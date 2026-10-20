#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "engine/graphics/Types.hpp"

template <typename Handle> class TextureTable {
public:
    engine::graphics::TextureId add(Handle handle) {
        const engine::graphics::TextureId id{_next++};
        _handles.emplace(id.value, std::move(handle));
        return id;
    }

    [[nodiscard]] const Handle* find(engine::graphics::TextureId id) const {
        const auto found = _handles.find(id.value);
        return found == _handles.end() ? nullptr : &found->second;
    }

    std::optional<Handle> remove(engine::graphics::TextureId id) {
        const auto found = _handles.find(id.value);
        if (found == _handles.end()) {
            return std::nullopt;
        }
        Handle handle = std::move(found->second);
        _handles.erase(found);
        return handle;
    }

    std::vector<Handle> remove_all() {
        std::vector<Handle> handles;
        handles.reserve(_handles.size());
        for (auto& [id, handle] : _handles) {
            handles.push_back(std::move(handle));
        }
        _handles.clear();
        return handles;
    }

private:
    std::unordered_map<std::uint32_t, Handle> _handles;
    std::uint32_t _next{1};
};
