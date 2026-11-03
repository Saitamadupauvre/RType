#pragma once

#include <cstddef>
#include <deque>
#include <optional>
#include <vector>

#include "engine/editor/SceneDocument.hpp"

namespace engine::editor {

class History {
public:
    explicit History(std::size_t limit) : _limit(limit) {}

    void record(SceneDocument before);
    std::optional<SceneDocument> undo(SceneDocument current);
    std::optional<SceneDocument> redo(SceneDocument current);
    void clear() noexcept;

    [[nodiscard]] bool can_undo() const noexcept { return !_undo.empty(); }
    [[nodiscard]] bool can_redo() const noexcept { return !_redo.empty(); }

private:
    std::size_t _limit;
    std::deque<SceneDocument> _undo;
    std::vector<SceneDocument> _redo;
};

} // namespace engine::editor
