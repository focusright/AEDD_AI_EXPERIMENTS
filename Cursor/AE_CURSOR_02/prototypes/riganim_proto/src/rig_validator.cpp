#include "rig_validator.h"

#include <cmath>
#include <cstdio>

#include "skeleton.h"

namespace aerigp1 {

std::vector<std::string> RigValidator::Validate(const RigDocument& doc) {
    std::vector<std::string> out;
    const Skeleton& sk = doc.skeleton;

    if (sk.joints.empty()) {
        out.push_back("Skeleton has no joints.");
        return out;
    }

    bool has_root = false;
    for (std::size_t i = 0; i < sk.joints.size(); ++i) {
        const Joint& j = sk.joints[i];
        if (j.parent < 0) {
            has_root = true;
        }
        if (j.parent >= static_cast<int>(sk.joints.size())) {
            out.push_back("Joint '" + j.name + "' has invalid parent index.");
        }
        if (SkeletonHasCycle(sk, static_cast<int>(i), j.parent)) {
            out.push_back("Hierarchy cycle detected at joint '" + j.name + "'.");
        }
    }
    if (!has_root) {
        out.push_back("No root joint (parent == -1).");
    }

    for (std::size_t vi = 0; vi < doc.mesh.vertices.size(); ++vi) {
        const SkinWeight& w = doc.mesh.vertices[vi].skin;
        float sum = 0.f;
        bool any = false;
        for (int i = 0; i < kMaxSkinInfluences; ++i) {
            sum += w.weights[i];
            if (w.weights[i] > 1e-5f) {
                any = true;
                if (w.joint_indices[i] < 0 || w.joint_indices[i] >= static_cast<int>(sk.joints.size())) {
                    out.push_back("Vertex " + std::to_string(vi) + " references invalid joint index.");
                }
            }
        }
        if (!any) {
            out.push_back("Vertex " + std::to_string(vi) + " has no skin weights.");
        }
        if (std::abs(sum - 1.f) > 0.02f && sum > 1e-5f) {
            out.push_back("Vertex " + std::to_string(vi) + " weights not normalized (sum=" + std::to_string(sum) + ").");
        }
    }

    for (const AnimClip& clip : doc.clips) {
        for (const AnimTrack& tr : clip.tracks) {
            if (tr.joint_index < 0 || tr.joint_index >= static_cast<int>(sk.joints.size())) {
                out.push_back("Clip '" + clip.name + "' track targets invalid joint " + std::to_string(tr.joint_index) + ".");
            }
        }
    }

    if (out.empty()) {
        out.push_back("Validation OK.");
    }
    return out;
}

void RigValidator::PrintToDebug(const std::vector<std::string>& messages) {
    for (const std::string& m : messages) {
        std::printf("[AERIGP1] %s\n", m.c_str());
    }
}

} // namespace aerigp1
