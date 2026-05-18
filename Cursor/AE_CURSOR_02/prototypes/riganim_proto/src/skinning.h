#pragma once

#include "rig_types.h"

namespace aerigp1 {

void SkinningNormalizeWeights(SkinWeight& w);
void SkinningNormalizeAll(RigDocument& doc);
void SkinningAutoWeightNearestJoints(RigDocument& doc, int max_influences);
void SkinningCpuDeform(RigDocument& doc, int highlight_joint);
DirectX::XMFLOAT4 SkinningWeightColor(const SkinWeight& w, int joint_index);

} // namespace aerigp1
