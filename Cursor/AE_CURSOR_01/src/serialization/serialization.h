#pragma once

#include <string>

#include "animation/animation_data.h"
#include "scene/scene_data.h"

namespace aetdp1::serialization {

struct LoadResult {
    bool ok{false};
    std::string error;
};

LoadResult LoadTextDocument(const std::string& text, scene::SceneData& out_scene, animation::AnimationDocument& out_anim);

// Human-legible line-oriented text format (prototype; not a standard).
std::string SaveTextDocument(const scene::SceneData& scene, const animation::AnimationDocument& anim);

} // namespace aetdp1::serialization
