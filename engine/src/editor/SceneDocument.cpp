#include "engine/editor/SceneDocument.hpp"

#include <algorithm>

namespace engine::editor {

const DocumentInstance* SceneDocument::find(InstanceId id) const noexcept {
    const auto found = std::ranges::find(instances, id, &DocumentInstance::id);
    return found == instances.end() ? nullptr : &*found;
}

DocumentInstance* SceneDocument::find(InstanceId id) noexcept {
    const auto found = std::ranges::find(instances, id, &DocumentInstance::id);
    return found == instances.end() ? nullptr : &*found;
}

script::Scene SceneDocument::to_scene() const {
    script::Scene scene{.instances = {}, .unknown_instances = unknown_instances};
    scene.instances.reserve(instances.size());
    for (const DocumentInstance& entry : instances) {
        scene.instances.push_back(entry.instance);
    }
    return scene;
}

} // namespace engine::editor
