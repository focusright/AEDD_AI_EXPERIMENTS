#include "scene_data.h"

#include <algorithm>

namespace aetdp1::scene {

static void InitUnitCube(BoxCorners& b) {
    // Standard axis-aligned unit cube from -0.5..0.5 (keeps gizmo pivot near object center).
    const DirectX::XMFLOAT3 corners[8] = {
        {-0.5f, -0.5f, -0.5f}, {+0.5f, -0.5f, -0.5f}, {+0.5f, +0.5f, -0.5f}, {-0.5f, +0.5f, -0.5f},
        {-0.5f, -0.5f, +0.5f}, {+0.5f, -0.5f, +0.5f}, {+0.5f, +0.5f, +0.5f}, {-0.5f, +0.5f, +0.5f},
    };
    for (int i = 0; i < 8; ++i) {
        b.v[i] = corners[i];
    }
}

void SceneData::EnsureDefaultBoxCorners(ObjectData& obj) {
    // Heuristic: if all verts are zero, treat as uninitialized.
    bool all_zero = true;
    for (int i = 0; i < 8; ++i) {
        const auto& p = obj.local_box.v[i];
        if (p.x != 0.f || p.y != 0.f || p.z != 0.f) {
            all_zero = false;
            break;
        }
    }
    if (all_zero) {
        InitUnitCube(obj.local_box);
    }
}

ObjectData* SceneData::TryGet(ObjectId id) {
    const int idx = IndexOf(id);
    return idx >= 0 ? &objects[static_cast<std::size_t>(idx)] : nullptr;
}

const ObjectData* SceneData::TryGet(ObjectId id) const {
    const int idx = IndexOf(id);
    return idx >= 0 ? &objects[static_cast<std::size_t>(idx)] : nullptr;
}

int SceneData::IndexOf(ObjectId id) const {
    for (std::size_t i = 0; i < objects.size(); ++i) {
        if (objects[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

ObjectId SceneData::AllocateId() {
    return next_id++;
}

void SceneData::RemoveById(ObjectId id) {
    objects.erase(std::remove_if(objects.begin(), objects.end(), [id](const ObjectData& o) { return o.id == id; }),
                  objects.end());
}

} // namespace aetdp1::scene
