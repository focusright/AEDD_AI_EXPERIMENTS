#pragma once

#include "rig_types.h"

namespace aerigp1 {

void AnimationEvaluateClip(const AnimClip& clip, int frame, Skeleton& skel);
bool AnimationSetKeyframe(RigDocument& doc, int joint_index, int frame);
bool AnimationDeleteKeyframe(RigDocument& doc, int joint_index, int frame);
void AnimationTickPlayback(RigDocument& doc, float dt_seconds);

} // namespace aerigp1
