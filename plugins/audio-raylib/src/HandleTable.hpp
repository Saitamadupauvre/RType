#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

template <typename Id, typename Handle> class HandleTable {
public:
    Id add(Handle handle) {
        const Id id{_next++};
        _handles.emplace(id.value, std::move(handle));
        return id;
    }

    [[nodiscard]] Handle* find(Id id) {
        const auto found = _handles.find(id.value);
        return found == _handles.end() ? nullptr : &found->second;
    }

    std::optional<Handle> remove(Id id) {
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
