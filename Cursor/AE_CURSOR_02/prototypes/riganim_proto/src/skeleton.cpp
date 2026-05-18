#include "skeleton.h"

#include "transform.h"

namespace aerigp1 {
namespace {
using namespace DirectX;
}

void SkeletonUpdateWorldTransforms(Skeleton& skel, bool use_pose) {
    for (Joint& j : skel.joints) {
        const Transform& local = use_pose ? j.local_pose : j.local_bind;
        const XMMATRIX local_m = TransformToMatrix(local);
        XMMATRIX world = local_m;
        if (j.parent >= 0 && j.parent < static_cast<int>(skel.joints.size())) {
            const XMMATRIX parent_w = XMLoadFloat4x4(use_pose ? &skel.joints[static_cast<std::size_t>(j.parent)].world_pose : &skel.joints[static_cast<std::size_t>(j.parent)].world_bind);
            world = XMMatrixMultiply(local_m, parent_w);
        }
        if (use_pose) {
            XMStoreFloat4x4(&j.world_pose, world);
        } else {
            XMStoreFloat4x4(&j.world_bind, world);
        }
    }
}

void SkeletonRecomputeInverseBind(Skeleton& skel) {
    SkeletonUpdateWorldTransforms(skel, false);
    for (Joint& j : skel.joints) {
        const XMMATRIX w = XMLoadFloat4x4(&j.world_bind);
        const XMMATRIX inv = XMMatrixInverse(nullptr, w);
        XMStoreFloat4x4(&j.inverse_bind, inv);
    }
}

void SkeletonCopyPoseFromBind(Skeleton& skel) {
    for (Joint& j : skel.joints) {
        j.local_pose = j.local_bind;
    }
    SkeletonUpdateWorldTransforms(skel, true);
}

void SkeletonApplyBindAsPose(Skeleton& skel) {
    for (Joint& j : skel.joints) {
        j.local_bind = j.local_pose;
    }
    SkeletonRecomputeInverseBind(skel);
    SkeletonCopyPoseFromBind(skel);
}

int SkeletonAddJoint(Skeleton& skel, const char* name, int parent) {
    Joint j{};
    j.name = name ? name : "joint";
    j.parent = parent;
    j.local_bind.scale = {1.f, 1.f, 1.f};
    j.local_bind.rotation = {0.f, 0.f, 0.f, 1.f};
    j.local_pose = j.local_bind;
    const int idx = static_cast<int>(skel.joints.size());
    skel.joints.push_back(j);
    if (parent < 0) {
        skel.root_index = idx;
    }
    SkeletonUpdateWorldTransforms(skel, false);
    SkeletonRecomputeInverseBind(skel);
    SkeletonCopyPoseFromBind(skel);
    return idx;
}

bool SkeletonDeleteJoint(Skeleton& skel, int index) {
    if (index < 0 || index >= static_cast<int>(skel.joints.size())) {
        return false;
    }
    for (Joint& j : skel.joints) {
        if (j.parent == index) {
            return false;
        }
        if (j.parent > index) {
            --j.parent;
        }
    }
    skel.joints.erase(skel.joints.begin() + index);
    if (skel.selected == index) {
        skel.selected = skel.joints.empty() ? -1 : 0;
    } else if (skel.selected > index) {
        --skel.selected;
    }
    if (skel.root_index == index) {
        skel.root_index = skel.joints.empty() ? -1 : 0;
    } else if (skel.root_index > index) {
        --skel.root_index;
    }
    SkeletonUpdateWorldTransforms(skel, false);
    SkeletonRecomputeInverseBind(skel);
    SkeletonCopyPoseFromBind(skel);
    return true;
}

bool SkeletonReparentJoint(Skeleton& skel, int index, int new_parent) {
    if (index < 0 || index >= static_cast<int>(skel.joints.size())) {
        return false;
    }
    if (new_parent >= static_cast<int>(skel.joints.size())) {
        return false;
    }
    if (new_parent == index) {
        return false;
    }
    if (SkeletonHasCycle(skel, index, new_parent)) {
        return false;
    }
    skel.joints[static_cast<std::size_t>(index)].parent = new_parent;
    if (new_parent < 0) {
        skel.root_index = index;
    }
    SkeletonUpdateWorldTransforms(skel, true);
    return true;
}

bool SkeletonHasCycle(const Skeleton& skel, int joint, int new_parent) {
    int p = new_parent;
    while (p >= 0) {
        if (p == joint) {
            return true;
        }
        p = skel.joints[static_cast<std::size_t>(p)].parent;
    }
    return false;
}

} // namespace aerigp1
