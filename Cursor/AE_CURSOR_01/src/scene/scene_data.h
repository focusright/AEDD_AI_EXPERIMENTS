#pragma once

#include <array>
#include <string>
#include <vector>

#include "core/types.h"

namespace aetdp1::scene {

// Local-space corners for a deformed box prototype (8 verts). Defaults to a unit cube.
struct BoxCorners {
    DirectX::XMFLOAT3 v[8]{};
};

// Per-face RGB colors for the local box primitive.
// Face order: -Y, +Y, -Z, +Z, -X, +X
struct FaceColors {
    std::array<DirectX::XMFLOAT3, 6> rgb{{
        {0.80f, 0.35f, 0.35f},
        {0.35f, 0.80f, 0.35f},
        {0.35f, 0.35f, 0.80f},
        {0.85f, 0.80f, 0.30f},
        {0.80f, 0.35f, 0.80f},
        {0.30f, 0.80f, 0.80f},
    }};
};

struct ObjectData {
    ObjectId id{kInvalidObjectId};
    std::string name;

    // RGB in 0..1 (prototype visualization only).
    DirectX::XMFLOAT3 color{0.7f, 0.7f, 0.75f};

    Transform transform{};
    BoxCorners local_box{};
    FaceColors face_colors{};

    // Which local vertex is considered "selected" for the tiny geometry-edit demo (0..7).
    std::uint8_t selected_vertex_index{0};
};

struct SceneData {
    std::vector<ObjectData> objects;
    ObjectId next_id{1};

    ObjectData* TryGet(ObjectId id);
    const ObjectData* TryGet(ObjectId id) const;
    int IndexOf(ObjectId id) const;

    ObjectId AllocateId();
    void RemoveById(ObjectId id);

    // Initializes default unit-cube corners if they look uninitialized.
    static void EnsureDefaultBoxCorners(ObjectData& obj);
};

} // namespace aetdp1::scene
