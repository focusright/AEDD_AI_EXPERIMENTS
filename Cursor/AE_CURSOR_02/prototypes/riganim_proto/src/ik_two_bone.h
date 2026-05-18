#pragma once

#include "rig_types.h"

namespace aerigp1 {

// Simple 2-bone IK: rotates upper and lower joints so end effector reaches target (in world space).
void IkSolveTwoBone(Skeleton& skel, int upper_index, int lower_index, int end_index, const DirectX::XMFLOAT3& target_world);

} // namespace aerigp1
