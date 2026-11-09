#include "render/ScissorStack.hpp"

#include "render/Geometry.hpp"

namespace editor {

Clay_BoundingBox ScissorStack::push(Clay_BoundingBox box) {
    const Clay_BoundingBox active = _boxes.empty() ? box : intersect(_boxes.back(), box);
    _boxes.push_back(active);
    return active;
}

std::optional<Clay_BoundingBox> ScissorStack::pop() {
    if (!_boxes.empty()) {
        _boxes.pop_back();
    }
    if (_boxes.empty()) {
        return std::nullopt;
    }
    return _boxes.back();
}

} // namespace editor
