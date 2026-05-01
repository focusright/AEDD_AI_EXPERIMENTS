#pragma once

#include "animation/animation_data.h"
#include "scene/scene_data.h"

namespace aetdp1::animation {

// Writes evaluated transforms into scene objects for the prototype's "pose preview" path.
// This is intentionally direct (prototype glue): a production tool might write into a separate pose buffer.
void EvaluateTransformTracksAtTime(const AnimationDocument& doc, scene::SceneData& scene, float time_seconds);

} // namespace aetdp1::animation
