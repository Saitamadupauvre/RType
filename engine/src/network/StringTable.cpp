#include "engine/network/StringTable.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace engine::network {

namespace {

bool has_valid_length(std::string_view name) noexcept {
    return !name.empty() && name.size() <= max_string_name_length;
}

auto in_category(StringCategory category) {
    return [category](const StringEntry& entry) { return entry.category == category; };
}

} // namespace

std::optional<std::uint16_t> StringTable::add(StringCategory category, std::string name) {
    if (const auto existing = id_of(category, name)) {
        return existing;
    }
    std::uint16_t next_id = 1;
    for (const StringEntry& entry : _entries) {
        if (entry.category == category && entry.id >= next_id) {
            next_id = static_cast<std::uint16_t>(entry.id + 1);
        }
    }
    if (!insert(StringEntry{.category = category, .id = next_id, .name = std::move(name)})) {
        return std::nullopt;
    }
    return next_id;
}

bool StringTable::insert(StringEntry entry) {
    if (!has_valid_length(entry.name) || entry.id == reserved_string_id ||
        name_of(entry.category, entry.id) != nullptr || id_of(entry.category, entry.name)) {
        return false;
    }
    _entries.push_back(std::move(entry));
    return true;
}

std::optional<std::uint16_t> StringTable::id_of(StringCategory category,
                                                std::string_view name) const {
    const auto found = std::ranges::find_if(_entries, [&](const StringEntry& entry) {
        return entry.category == category && entry.name == name;
    });
    if (found == _entries.end()) {
        return std::nullopt;
    }
    return found->id;
}

const std::string* StringTable::name_of(StringCategory category, std::uint16_t id) const {
    const auto found = std::ranges::find_if(_entries, [&](const StringEntry& entry) {
        return entry.category == category && entry.id == id;
    });
    return found == _entries.end() ? nullptr : &found->name;
}

std::vector<StringEntry> StringTable::entries(StringCategory category) const {
    std::vector<StringEntry> selected;
    std::ranges::copy_if(_entries, std::back_inserter(selected), in_category(category));
    return selected;
}

} // namespace engine::network
