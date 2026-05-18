#include "skinning.h"

#include <algorithm>
#include <cmath>

#include "skeleton.h"
#include "transform.h"

namespace aerigp1 {
namespace {
using namespace DirectX;

static float DistSq(const XMFLOAT3& a, const XMFLOAT3& b) {
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

static XMFLOAT3 JointWorldPos(const Skeleton& skel, int joint_index) {
    const Joint& j = skel.joints[static_cast<std::size_t>(joint_index)];
    XMFLOAT3 p{};
    XMStoreFloat3(&p, XMVector3TransformCoord(XMVectorZero(), XMLoadFloat4x4(&j.world_pose)));
    return p;
}

} // namespace

void SkinningNormalizeWeights(SkinWeight& w) {
    float sum = 0.f;
    for (int i = 0; i < kMaxSkinInfluences; ++i) {
        sum += std::max(0.f, w.weights[i]);
    }
    if (sum < 1e-6f) {
        return;
    }
    for (int i = 0; i < kMaxSkinInfluences; ++i) {
        w.weights[i] /= sum;
    }
}

void SkinningNormalizeAll(RigDocument& doc) {
    for (Vertex& v : doc.mesh.vertices) {
        SkinningNormalizeWeights(v.skin);
    }
}

void SkinningAutoWeightNearestJoints(RigDocument& doc, int max_influences) {
    const int n = std::min(max_influences, kMaxSkinInfluences);
    SkeletonUpdateWorldTransforms(doc.skeleton, true);
    for (Vertex& v : doc.mesh.vertices) {
        struct Entry {
            int joint;
            float dist;
        };
        std::vector<Entry> entries;
        entries.reserve(doc.skeleton.joints.size());
        for (int ji = 0; ji < static_cast<int>(doc.skeleton.joints.size()); ++ji) {
            const float d = DistSq(v.position, JointWorldPos(doc.skeleton, ji));
            entries.push_back({ji, d});
        }
        std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.dist < b.dist; });
        SkinWeight sw{};
        float inv_sum = 0.f;
        for (int i = 0; i < n && i < static_cast<int>(entries.size()); ++i) {
            sw.joint_indices[i] = entries[static_cast<std::size_t>(i)].joint;
            const float w = 1.f / (std::sqrt(entries[static_cast<std::size_t>(i)].dist) + 1e-3f);
            sw.weights[i] = w;
            inv_sum += w;
        }
        for (int i = 0; i < kMaxSkinInfluences; ++i) {
            if (inv_sum > 0.f) {
                sw.weights[i] /= inv_sum;
            }
        }
        v.skin = sw;
    }
}

void SkinningCpuDeform(RigDocument& doc, int highlight_joint) {
    doc.skinned_vertices = doc.mesh.vertices;
    SkeletonUpdateWorldTransforms(doc.skeleton, true);

    std::vector<XMMATRIX> skin_mats(doc.skeleton.joints.size());
    for (std::size_t ji = 0; ji < doc.skeleton.joints.size(); ++ji) {
        const XMMATRIX pose = XMLoadFloat4x4(&doc.skeleton.joints[ji].world_pose);
        const XMMATRIX inv_bind = XMLoadFloat4x4(&doc.skeleton.joints[ji].inverse_bind);
        skin_mats[ji] = XMMatrixMultiply(inv_bind, pose);
    }

    for (std::size_t vi = 0; vi < doc.skinned_vertices.size(); ++vi) {
        Vertex& out = doc.skinned_vertices[vi];
        const Vertex& bind = doc.mesh.vertices[vi];
        XMVECTOR pos = XMVectorZero();
        XMVECTOR nrm = XMVectorZero();
        for (int i = 0; i < kMaxSkinInfluences; ++i) {
            const float w = bind.skin.weights[i];
            const int ji = bind.skin.joint_indices[i];
            if (w <= 0.f || ji < 0 || ji >= static_cast<int>(skin_mats.size())) {
                continue;
            }
            const XMVECTOR p = XMVector3TransformCoord(XMLoadFloat3(&bind.position), skin_mats[static_cast<std::size_t>(ji)]);
            const XMVECTOR n = XMVector3TransformNormal(XMLoadFloat3(&bind.normal), skin_mats[static_cast<std::size_t>(ji)]);
            pos = XMVectorMultiplyAdd(XMVectorReplicate(w), p, pos);
            nrm = XMVectorMultiplyAdd(XMVectorReplicate(w), n, nrm);
        }
        XMStoreFloat3(&out.position, pos);
        XMStoreFloat3(&out.normal, XMVector3Normalize(nrm));
        if (highlight_joint >= 0 && doc.show_weight_debug) {
            out.color = SkinningWeightColor(bind.skin, highlight_joint);
        } else {
            out.color = bind.color;
        }
    }
}

XMFLOAT4 SkinningWeightColor(const SkinWeight& w, int joint_index) {
    float sum = 0.f;
    for (int i = 0; i < kMaxSkinInfluences; ++i) {
        if (w.joint_indices[i] == joint_index) {
            sum += w.weights[i];
        }
    }
    return {sum, sum * 0.3f, 1.f - sum, 1.f};
}

} // namespace aerigp1
