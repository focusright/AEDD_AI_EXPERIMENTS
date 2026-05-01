#include "animation_data.h"

#include <algorithm>

namespace aetdp1::animation {

int AnimationDocument::FindTrackIndex(ObjectId id) const {
    for (int i = 0; i < static_cast<int>(tracks.size()); ++i) {
        if (tracks[static_cast<std::size_t>(i)].target == id) {
            return i;
        }
    }
    return -1;
}

TransformTrack* AnimationDocument::TryGetTrack(ObjectId id) {
    const int idx = FindTrackIndex(id);
    return idx >= 0 ? &tracks[static_cast<std::size_t>(idx)] : nullptr;
}

const TransformTrack* AnimationDocument::TryGetTrack(ObjectId id) const {
    const int idx = FindTrackIndex(id);
    return idx >= 0 ? &tracks[static_cast<std::size_t>(idx)] : nullptr;
}

static void SortKeys(TransformTrack& tr) {
    std::sort(tr.keys.begin(), tr.keys.end(), [](const TransformKeyframe& a, const TransformKeyframe& b) {
        return a.time_seconds < b.time_seconds;
    });
}

void AnimationDocument::UpsertKeyframe(ObjectId id, const TransformKeyframe& key) {
    TransformTrack* tr = TryGetTrack(id);
    if (!tr) {
        TransformTrack nt;
        nt.target = id;
        nt.keys.push_back(key);
        tracks.push_back(std::move(nt));
        tr = &tracks.back();
        SortKeys(*tr);
        return;
    }

    bool replaced = false;
    for (auto& k : tr->keys) {
        if (k.time_seconds == key.time_seconds) {
            k = key;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        tr->keys.push_back(key);
    }
    SortKeys(*tr);
}

bool AnimationDocument::TryDeleteKeyframeAtTime(ObjectId id, float time_seconds) {
    TransformTrack* tr = TryGetTrack(id);
    if (!tr) {
        return false;
    }
    const auto it = std::remove_if(tr->keys.begin(), tr->keys.end(),
                                   [time_seconds](const TransformKeyframe& k) { return k.time_seconds == time_seconds; });
    if (it == tr->keys.end()) {
        return false;
    }
    tr->keys.erase(it, tr->keys.end());
    return true;
}

} // namespace aetdp1::animation
