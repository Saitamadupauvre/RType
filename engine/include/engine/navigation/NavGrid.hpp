#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "engine/navigation/Export.hpp"

namespace engine::navigation {

/** @brief Grid coordinates of a cell: x to the right, y downward, from 0. */
struct Cell {
    int x{0};
    int y{0};

    friend bool operator==(Cell, Cell) = default;
};

/** @brief A point in world units. */
struct Point {
    float x{0.0F};
    float y{0.0F};

    friend bool operator==(Point, Point) = default;
};

/**
 * @brief Rectangular grid of cells laid over an area of the world, used to find paths.
 *
 * Cell (x, y) covers the world rectangle from `origin + (x, y) * cell_size` (included) to
 * `origin + (x + 1, y + 1) * cell_size` (excluded). Every cell is either blocked or walkable with
 * a movement cost of at least 1 (the cost of entering it); a new grid is fully walkable with
 * cost 1.
 *
 * @note Not thread safe. The grid knows nothing about entities: callers block cells from whatever
 * describes their obstacles, for example with block_area().
 */
class ENGINE_NAVIGATION_EXPORT NavGrid {
public:
    /** @brief Largest number of cells a grid may hold. */
    static constexpr std::size_t max_cells = std::size_t{1} << 20U;

    /**
     * @brief Creates a fully walkable grid.
     *
     * @param width Number of columns, positive.
     * @param height Number of rows, positive.
     * @param cell_size Side of a cell in world units, positive and finite.
     * @param origin World position of the top-left corner of cell (0, 0), finite.
     * @throws std::invalid_argument If a size is not positive, the grid would hold more than
     * max_cells cells, or a value is not finite.
     */
    NavGrid(int width, int height, float cell_size, Point origin = {});

    /** @return Number of columns. */
    [[nodiscard]] int width() const noexcept;

    /** @return Number of rows. */
    [[nodiscard]] int height() const noexcept;

    /** @return Side of a cell in world units. */
    [[nodiscard]] float cell_size() const noexcept;

    /** @return World position of the top-left corner of cell (0, 0). */
    [[nodiscard]] Point origin() const noexcept;

    /** @return true if the cell is inside the grid. */
    [[nodiscard]] bool contains(Cell cell) const noexcept;

    /** @return true if the cell is inside the grid and not blocked. */
    [[nodiscard]] bool walkable(Cell cell) const noexcept;

    /** @return Cost of entering a walkable cell, 0 for a blocked cell or a cell outside. */
    [[nodiscard]] float cost(Cell cell) const noexcept;

    /**
     * @brief Blocks or unblocks a cell. Unblocking keeps the cell's cost.
     * @throws std::out_of_range If the cell is outside the grid.
     */
    void set_blocked(Cell cell, bool blocked);

    /**
     * @brief Sets the cost of entering a cell, blocked or not.
     *
     * @param cost At least 1, so the distance heuristics never overestimate.
     * @throws std::out_of_range If the cell is outside the grid.
     * @throws std::invalid_argument If the cost is below 1 or not finite.
     */
    void set_cost(Cell cell, float cost);

    /**
     * @brief Blocks every cell the world rectangle overlaps.
     *
     * Overlap is strict: a rectangle only touching a cell's edge does not block it. Parts outside
     * the grid are ignored, as are empty or non-finite rectangles.
     *
     * @param min Top-left corner in world units.
     * @param max Bottom-right corner in world units.
     */
    void block_area(Point min, Point max) noexcept;

    /** @brief Makes every cell walkable with cost 1. */
    void reset() noexcept;

    /** @return The cell holding a world point, or nullopt outside the grid or for a non-finite
     * point. */
    [[nodiscard]] std::optional<Cell> cell_at(Point point) const noexcept;

    /** @return World position of the centre of a cell (also for cells outside the grid). */
    [[nodiscard]] Point center_of(Cell cell) const noexcept;

private:
    [[nodiscard]] std::size_t index(Cell cell) const noexcept;
    [[nodiscard]] std::size_t checked_index(Cell cell) const;

    int _width;
    int _height;
    float _cell_size;
    Point _origin;
    std::vector<float> _costs;
    std::vector<std::uint8_t> _blocked;
};

} // namespace engine::navigation
