#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/network/Export.hpp"

namespace engine::network {

/**
 * @brief Kind of name in a StringTable. Each category has its own id space.
 *
 * The values are sent on the wire: never reorder them.
 */
enum class StringCategory : std::uint8_t {
    /** @brief Entity type: `Replicated::type_id`, `EntitySpawned::type_id`. */
    type = 0,
    /** @brief Sound: `SoundPlayed::sound_id`. */
    sound = 1,
    /** @brief Music: `MusicChanged::music_id`. */
    music = 2,
    /** @brief Game event name. */
    event = 3,
    /** @brief Shader name. */
    shader = 4,
    /** @brief Replicated component field name. */
    field = 5,
};

/** @brief Number of StringCategory values; a category byte at or above it is invalid. */
inline constexpr std::size_t string_category_count = 6;

/** @brief Id reserved in every category, never given to a name (`no_music` for musics). */
inline constexpr std::uint16_t reserved_string_id = 0xFFFF;

/** @brief Longest name a StringTable accepts, in bytes. */
inline constexpr std::size_t max_string_name_length = 63;

/**
 * @brief One name of a StringTable and the id it is sent as.
 */
struct ENGINE_NETWORK_EXPORT StringEntry {
    StringCategory category;
    std::uint16_t id;
    std::string name;

    bool operator==(const StringEntry&) const = default;
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/**
 * @brief Names the server sends once in `connect_accept`, so later packets carry small ids
 * instead of names.
 *
 * Within one category, ids and names are unique; the same id may name different things in two
 * categories. Names are 1 to `max_string_name_length` bytes long and no name uses
 * `reserved_string_id`. Every member keeps these rules: a table is always valid, but it may
 * still be too large to fit in one packet, which NetworkServer::start() reports.
 *
 * @note Not thread-safe. Lookups are linear in the number of entries.
 */
class ENGINE_NETWORK_EXPORT StringTable {
public:
    /**
     * @brief Gives @p name the next free id of @p category, the server-side way to fill a table.
     *
     * Ids start at 1 and follow the order of the calls.
     *
     * @return The id of @p name, the existing one when it is already in @p category, or
     * `std::nullopt` when the name is empty, longer than `max_string_name_length`, or the
     * category has no id left.
     */
    std::optional<std::uint16_t> add(StringCategory category, std::string name);

    /**
     * @brief Adds an entry with a chosen id, the client-side way to rebuild a received table.
     *
     * @return `false`, leaving the table unchanged, when the name length is invalid, the id is
     * `reserved_string_id`, or the category already has this id or this name.
     */
    bool insert(StringEntry entry);

    /**
     * @return The id of @p name in @p category, or `std::nullopt` when absent.
     */
    [[nodiscard]] std::optional<std::uint16_t> id_of(StringCategory category,
                                                     std::string_view name) const;

    /**
     * @return The name of @p id in @p category, or `nullptr` when absent. The pointer stays
     * valid until the table is changed or destroyed.
     */
    [[nodiscard]] const std::string* name_of(StringCategory category, std::uint16_t id) const;

    /**
     * @return The names of @p category, in insertion order.
     */
    [[nodiscard]] std::vector<StringEntry> entries(StringCategory category) const;

    /**
     * @return Every entry, in insertion order.
     */
    [[nodiscard]] const std::vector<StringEntry>& entries() const noexcept { return _entries; }

    bool operator==(const StringTable&) const = default;

private:
    std::vector<StringEntry> _entries;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace engine::network
