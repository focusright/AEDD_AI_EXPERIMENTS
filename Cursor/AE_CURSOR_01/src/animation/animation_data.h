#pragma once

#include <vector>

#include "core/types.h"

namespace aetdp1::animation {

struct TransformKeyframe {
    float time_seconds{0.f};
    Transform value{};
};

struct TransformTrack {
    ObjectId target{kInvalidObjectId};
    std::vector<TransformKeyframe> keys; // sorted by time_seconds
};

struct PlaybackState {
    bool playing{false};
    bool paused{false};
};

struct AnimationDocument {
    float duration_seconds{10.f};
    float current_time_seconds{0.f};
    PlaybackState playback{};
    std::vector<TransformTrack> tracks;

    int FindTrackIndex(ObjectId id) const;
    TransformTrack* TryGetTrack(ObjectId id);
    const TransformTrack* TryGetTrack(ObjectId id) const;

    // Inserts or replaces a key at the exact time (stable ordering).
    void UpsertKeyframe(ObjectId id, const TransformKeyframe& key);

    bool TryDeleteKeyframeAtTime(ObjectId id, float time_seconds);
};

} // namespace aetdp1::animation
