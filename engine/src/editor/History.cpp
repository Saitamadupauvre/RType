#include "History.hpp"

#include <utility>

namespace engine::editor {

void History::record(SceneDocument before) {
    _undo.push_back(std::move(before));
    while (_undo.size() > _limit) {
        _undo.pop_front();
    }
    _redo.clear();
}

std::optional<SceneDocument> History::undo(SceneDocument current) {
    if (_undo.empty()) {
        return std::nullopt;
    }
    SceneDocument previous = std::move(_undo.back());
    _undo.pop_back();
    _redo.push_back(std::move(current));
    return previous;
}

std::optional<SceneDocument> History::redo(SceneDocument current) {
    if (_redo.empty()) {
        return std::nullopt;
    }
    SceneDocument next = std::move(_redo.back());
    _redo.pop_back();
    _undo.push_back(std::move(current));
    return next;
}

void History::clear() noexcept {
    _undo.clear();
    _redo.clear();
}

} // namespace engine::editor
