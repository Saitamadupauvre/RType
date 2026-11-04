#pragma once

#include <clay.h>

#include <optional>
#include <vector>

namespace editor {

class ScissorStack {
public:
    Clay_BoundingBox push(Clay_BoundingBox box);
    std::optional<Clay_BoundingBox> pop();

private:
    std::vector<Clay_BoundingBox> boxes_;
};

} // namespace editor
