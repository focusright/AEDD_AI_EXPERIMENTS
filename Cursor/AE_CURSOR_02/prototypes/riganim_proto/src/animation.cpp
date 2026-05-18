#include "animation.h"

#include <algorithm>

#include "skeleton.h"
#include "transform.h"

namespace aerigp1 {
namespace {
using namespace DirectX;

static bool FindBracket(const AnimTrack& tr, int frame, int& out_before, int& out_after) {
    if (tr.keys.empty()) {
        return false;
    }
    if (frame <= tr.keys.front().frame) {
        out_before = out_after = 0;
        return true;
    }
    if (frame >= tr.keys.back().frame) {
        out_before = out_after = static_cast<int>(tr.keys.size()) - 1;
        return true;
    }
    for (int i = 0; i + 1 < static_cast<int>(tr.keys.size()); ++i) {
        const int f0 = tr.keys[static_cast<std::size_t>(i)].frame;
        const int f1 = tr.keys[static_cast<std::size_t>(i + 1)].frame;
        if (f0 <= frame && frame <= f1) {
            out_before = i;
            out_after = i + 1;
            return true;
        }
    }
    out_before = out_after = static_cast<int>(tr.keys.size()) - 1;
    return true;
}

static Transform EvalTrack(const AnimTrack& tr, int frame) {
    int ib = 0, ia = 0;
    if (!FindBracket(tr, frame, ib, ia)) {
        return Transform{};
    }
    const AnimKey& kb = tr.keys[static_cast<std::size_t>(ib)];
    const AnimKey& ka = tr.keys[static_cast<std::size_t>(ia)];
    if (ib == ia) {
        return kb.local;
    }
    const float f0 = static_cast<float>(kb.frame);
    const float f1 = static_cast<float>(ka.frame);
    const float denom = f1 - f0;
    const float u = denom > 1e-6f ? (static_cast<float>(frame) - f0) / denom : 0.f;
    return LerpTransform(kb.local, ka.local, std::clamp(u, 0.f, 1.f));
}

} // namespace

void AnimationEvaluateClip(const AnimClip& clip, int frame, Skeleton& skel) {
    for (const AnimTrack& tr : clip.tracks) {
        if (tr.joint_index < 0 || tr.joint_index >= static_cast<int>(skel.joints.size())) {
            continue;
        }
        if (tr.keys.empty()) {
            continue;
        }
        skel.joints[static_cast<std::size_t>(tr.joint_index)].local_pose = EvalTrack(tr, frame);
    }
    SkeletonUpdateWorldTransforms(skel, true);
}

bool AnimationSetKeyframe(RigDocument& doc, int joint_index, int frame) {
    if (doc.clips.empty() || joint_index < 0) {
        return false;
    }
    AnimClip& clip = doc.clips[static_cast<std::size_t>(doc.active_clip)];
    AnimTrack* track = nullptr;
    for (AnimTrack& tr : clip.tracks) {
        if (tr.joint_index == joint_index) {
            track = &tr;
            break;
        }
    }
    if (!track) {
        AnimTrack tr{};
        tr.joint_index = joint_index;
        clip.tracks.push_back(tr);
        track = &clip.tracks.back();
    }
    for (AnimKey& k : track->keys) {
        if (k.frame == frame) {
            k.local = doc.skeleton.joints[static_cast<std::size_t>(joint_index)].local_pose;
            return true;
        }
    }
    AnimKey k{};
    k.frame = frame;
    k.local = doc.skeleton.joints[static_cast<std::size_t>(joint_index)].local_pose;
    track->keys.push_back(k);
    std::sort(track->keys.begin(), track->keys.end(), [](const AnimKey& a, const AnimKey& b) { return a.frame < b.frame; });
    return true;
}

bool AnimationDeleteKeyframe(RigDocument& doc, int joint_index, int frame) {
    if (doc.clips.empty()) {
        return false;
    }
    AnimClip& clip = doc.clips[static_cast<std::size_t>(doc.active_clip)];
    for (AnimTrack& tr : clip.tracks) {
        if (tr.joint_index != joint_index) {
            continue;
        }
        const auto it = std::remove_if(tr.keys.begin(), tr.keys.end(), [&](const AnimKey& k) { return k.frame == frame; });
        if (it != tr.keys.end()) {
            tr.keys.erase(it, tr.keys.end());
            return true;
        }
    }
    return false;
}

void AnimationTickPlayback(RigDocument& doc, float dt_seconds) {
    if (!doc.playing || doc.clips.empty()) {
        return;
    }
    const AnimClip& clip = doc.clips[static_cast<std::size_t>(doc.active_clip)];
    doc.playback_accum += dt_seconds;
    const float frame_duration = 1.f / clip.fps;
    while (doc.playback_accum >= frame_duration) {
        doc.playback_accum -= frame_duration;
        ++doc.current_frame;
        if (doc.current_frame > clip.frame_count) {
            doc.current_frame = 0;
        }
    }
    AnimationEvaluateClip(clip, doc.current_frame, doc.skeleton);
}

} // namespace aerigp1
