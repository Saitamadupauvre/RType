#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "engine/navigation/Export.hpp"
#include "engine/navigation/NavGrid.hpp"

namespace engine::navigation {

/**
 * @brief Distance estimate A* uses to search toward the goal first.
 *
 * Dijkstra uses no estimate and explores evenly in every direction. Octile is exact on an open
 * grid with diagonal moves, Manhattan on an open grid without them; Euclidean is always safe but
 * explores more. Manhattan overestimates when diagonals are allowed: paths are found faster but
 * may be slightly longer than the shortest one.
 */
enum class Heuristic : std::uint8_t { Dijkstra, Manhattan, Octile, Euclidean };

/** @return The heuristic with that lowercase name ("dijkstra", "octile"...), or nullopt. */
[[nodiscard]] ENGINE_NAVIGATION_EXPORT std::optional<Heuristic>
parse_heuristic(std::string_view name) noexcept;

/** @return Lowercase name of a heuristic, as accepted by parse_heuristic. */
[[nodiscard]] ENGINE_NAVIGATION_EXPORT std::string_view name_of(Heuristic heuristic) noexcept;

/** @brief Default limit of cells a search may expand. */
inline constexpr std::size_t default_max_expanded = 10'000;

/** @brief How find_path searches. */
struct PathOptions {
    Heuristic heuristic{Heuristic::Octile};
    /** Allow diagonal moves; a diagonal needs both cells beside it walkable. */
    bool diagonals{true};
    /** Search budget: the search gives up after expanding this many cells. */
    std::size_t max_expanded{default_max_expanded};
    /** Drop the cells a straight line can skip, see smooth_path(). */
    bool smooth{false};
};

/** @brief Outcome of find_path. */
enum class PathStatus : std::uint8_t {
    Found,        ///< `cells` holds the path.
    Unreachable,  ///< Every reachable cell was explored without meeting the goal.
    LimitReached, ///< The search expanded `max_expanded` cells without meeting the goal.
    OutsideGrid,  ///< The start or the goal is outside the grid.
    Blocked,      ///< The start or the goal is blocked.
};

/** @brief Result of find_path. */
struct PathResult {
    PathStatus status{PathStatus::Unreachable};
    /** From start to goal, both included; empty unless `status` is Found. */
    std::vector<Cell> cells;
    /** Cost of the grid path before smoothing; 0 unless found. */
    float cost{0.0F};
    /** Number of cells the search expanded. */
    std::size_t expanded{0};
};

/**
 * @brief Finds the cheapest path between two cells with A*.
 *
 * Entering a cell costs its cost, times sqrt(2) for a diagonal move. The path is the cheapest one
 * when the heuristic does not overestimate (always true for Dijkstra, Octile and Euclidean; true
 * for Manhattan without diagonals). Equal start and goal give a one-cell path of cost 0.
 *
 * @note Deterministic: the same grid and arguments always give the same path.
 */
[[nodiscard]] ENGINE_NAVIGATION_EXPORT PathResult find_path(const NavGrid& grid, Cell start,
                                                            Cell goal,
                                                            const PathOptions& options = {});

} // namespace engine::navigation
