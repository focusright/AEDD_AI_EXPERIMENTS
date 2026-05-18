#include "ik_two_bone.h"

#include <algorithm>
#include <cmath>

#include "skeleton.h"
#include "transform.h"

namespace aerigp1 {
namespace {
using namespace DirectX;

static XMFLOAT3 JointWorldTranslation(const Joint& j) {
    XMFLOAT3 p{};
    XMStoreFloat3(&p, XMVector3TransformCoord(XMVectorZero(), XMLoadFloat4x4(&j.world_pose)));
    return p;
}

static XMVECTOR QuatBetweenVectors(XMVECTOR from_dir, XMVECTOR to_dir) {
    from_dir = XMVector3Normalize(from_dir);
    to_dir = XMVector3Normalize(to_dir);
    const XMVECTOR axis = XMVector3Cross(from_dir, to_dir);
    const float dot = XMVectorGetX(XMVector3Dot(from_dir, to_dir));
    if (dot > 0.9999f) {
        return XMQuaternionIdentity();
    }
    if (dot < -0.9999f) {
        XMVECTOR ortho = XMVector3Cross(from_dir, XMVectorSet(0.f, 1.f, 0.f, 0.f));
        if (XMVector3Equal(ortho, XMVectorZero())) {
            ortho = XMVector3Cross(from_dir, XMVectorSet(1.f, 0.f, 0.f, 0.f));
        }
        return XMQuaternionRotationAxis(XMVector3Normalize(ortho), XM_PI);
    }
    return XMQuaternionRotationAxis(XMVector3Normalize(axis), std::acos(std::clamp(dot, -1.f, 1.f)));
}

static void SetLocalRotationFromWorldDelta(Skeleton& skel, int joint_index, const XMMATRIX& desired_world_rot) {
    Joint& j = skel.joints[static_cast<std::size_t>(joint_index)];
    XMMATRIX parent_world = XMMatrixIdentity();
    if (j.parent >= 0) {
        parent_world = XMLoadFloat4x4(&skel.joints[static_cast<std::size_t>(j.parent)].world_pose);
    }
    const XMMATRIX parent_inv = XMMatrixInverse(nullptr, parent_world);
    const XMMATRIX local_m = XMMatrixMultiply(desired_world_rot, parent_inv);
    Transform t = j.local_pose;
    XMVECTOR s{}, r{}, p{};
    XMMatrixDecompose(&s, &r, &p, local_m);
    XMStoreFloat4(&t.rotation, XMQuaternionNormalize(r));
    j.local_pose = t;
}

} // namespace

void IkSolveTwoBone(Skeleton& skel, int upper_index, int lower_index, int end_index, const XMFLOAT3& target_world) {
    if (upper_index < 0 || lower_index < 0 || end_index < 0) {
        return;
    }
    if (upper_index >= static_cast<int>(skel.joints.size()) || lower_index >= static_cast<int>(skel.joints.size()) ||
        end_index >= static_cast<int>(skel.joints.size())) {
        return;
    }

    SkeletonUpdateWorldTransforms(skel, true);

    const XMFLOAT3 root_pos = JointWorldTranslation(skel.joints[static_cast<std::size_t>(upper_index)]);
    const XMFLOAT3 mid_pos = JointWorldTranslation(skel.joints[static_cast<std::size_t>(lower_index)]);
    const XMFLOAT3 end_pos = JointWorldTranslation(skel.joints[static_cast<std::size_t>(end_index)]);

    const float len_upper = XMVectorGetX(XMVector3Length(XMVectorSubtract(XMLoadFloat3(&mid_pos), XMLoadFloat3(&root_pos))));
    const float len_lower = XMVectorGetX(XMVector3Length(XMVectorSubtract(XMLoadFloat3(&end_pos), XMLoadFloat3(&mid_pos))));
    const float len_total = len_upper + len_lower;
    if (len_upper < 1e-4f || len_lower < 1e-4f) {
        return;
    }

    XMVECTOR target = XMLoadFloat3(&target_world);
    XMVECTOR root = XMLoadFloat3(&root_pos);
    XMVECTOR to_target = XMVectorSubtract(target, root);
    float dist = XMVectorGetX(XMVector3Length(to_target));
    dist = std::clamp(dist, std::abs(len_upper - len_lower) + 1e-3f, len_total - 1e-3f);

    // Law of cosines for elbow/knee angle at middle joint.
    const float cos_a = (len_upper * len_upper + dist * dist - len_lower * len_lower) / (2.f * len_upper * dist);
    const float angle_at_root = std::acos(std::clamp(cos_a, -1.f, 1.f));

    XMVECTOR dir = XMVector3Normalize(to_target);
    if (XMVector3Equal(dir, XMVectorZero())) {
        dir = XMVectorSet(0.f, 1.f, 0.f, 0.f);
    }

    // Bend plane: use character facing (Z) crossed with reach direction.
    XMVECTOR bend_axis = XMVector3Normalize(XMVector3Cross(dir, XMVectorSet(0.f, 0.f, 1.f, 0.f)));
    if (XMVector3Equal(bend_axis, XMVectorZero())) {
        bend_axis = XMVectorSet(0.f, 1.f, 0.f, 0.f);
    }

    const XMVECTOR upper_dir = XMVector3Normalize(XMVector3Rotate(dir, XMQuaternionRotationAxis(bend_axis, angle_at_root - XM_PIDIV2)));
    const XMVECTOR desired_mid = XMVectorAdd(root, XMVectorScale(upper_dir, len_upper));

    // Point upper bone toward desired_mid.
    const XMVECTOR cur_upper = XMVector3Normalize(XMVectorSubtract(XMLoadFloat3(&mid_pos), root));
    const XMVECTOR des_upper = XMVector3Normalize(XMVectorSubtract(desired_mid, root));
    const XMVECTOR q_upper = QuatBetweenVectors(cur_upper, des_upper);
    XMMATRIX upper_world = XMLoadFloat4x4(&skel.joints[static_cast<std::size_t>(upper_index)].world_pose);
    upper_world = XMMatrixMultiply(XMMatrixRotationQuaternion(q_upper), upper_world);
    SetLocalRotationFromWorldDelta(skel, upper_index, upper_world);
    SkeletonUpdateWorldTransforms(skel, true);

    const XMFLOAT3 mid_pos2 = JointWorldTranslation(skel.joints[static_cast<std::size_t>(lower_index)]);
    const XMVECTOR cur_lower = XMVector3Normalize(XMVectorSubtract(XMLoadFloat3(&end_pos), XMLoadFloat3(&mid_pos2)));
    const XMVECTOR des_lower = XMVector3Normalize(XMVectorSubtract(target, XMLoadFloat3(&mid_pos2)));
    const XMVECTOR q_lower = QuatBetweenVectors(cur_lower, des_lower);
    XMMATRIX lower_world = XMLoadFloat4x4(&skel.joints[static_cast<std::size_t>(lower_index)].world_pose);
    lower_world = XMMatrixMultiply(XMMatrixRotationQuaternion(q_lower), lower_world);
    SetLocalRotationFromWorldDelta(skel, lower_index, lower_world);
    SkeletonUpdateWorldTransforms(skel, true);
}

} // namespace aerigp1
