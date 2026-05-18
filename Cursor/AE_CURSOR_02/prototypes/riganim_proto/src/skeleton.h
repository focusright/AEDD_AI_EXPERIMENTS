#pragma once

#include "rig_types.h"

namespace aerigp1 {

void SkeletonUpdateWorldTransforms(Skeleton& skel, bool use_pose);
void SkeletonRecomputeInverseBind(Skeleton& skel);
void SkeletonCopyPoseFromBind(Skeleton& skel);
void SkeletonApplyBindAsPose(Skeleton& skel);

int SkeletonAddJoint(Skeleton& skel, const char* name, int parent);
bool SkeletonDeleteJoint(Skeleton& skel, int index);
bool SkeletonReparentJoint(Skeleton& skel, int index, int new_parent);
bool SkeletonHasCycle(const Skeleton& skel, int joint, int new_parent);

} // namespace aerigp1
