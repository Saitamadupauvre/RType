#include "render/ScissorStack.hpp"

#include "render/Geometry.hpp"

namespace editor {

Clay_BoundingBox ScissorStack::push(Clay_BoundingBox box) {
    const Clay_BoundingBox active = boxes_.empty() ? box : intersect(boxes_.back(), box);
    boxes_.push_back(active);
    return active;
}

std::optional<Clay_BoundingBox> ScissorStack::pop() {
    if (!boxes_.empty()) {
        boxes_.pop_back();
    }
    if (boxes_.empty()) {
        return std::nullopt;
    }
    return boxes_.back();
}

} // namespace editor
