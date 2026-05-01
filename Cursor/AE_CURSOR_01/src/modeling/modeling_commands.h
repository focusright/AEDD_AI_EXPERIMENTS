#pragma once

#include <memory>
#include <string>

#include "animation/animation_data.h"
#include "core/types.h"
#include "scene/scene_data.h"
#include "selection/selection_state.h"

namespace aetdp1::commands {

struct CommandContext {
    scene::SceneData* scene{nullptr};
    selection::SelectionState* selection{nullptr};
    animation::AnimationDocument* anim{nullptr};
};

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void Apply(CommandContext& ctx) = 0;
    virtual void Revert(CommandContext& ctx) = 0;
};

// ---- Semantic modeling helpers (owned by the Modeling specialist boundary) ----

scene::ObjectData MakeDefaultNamedObject(scene::SceneData& scene, const std::string& desired_name);

} // namespace aetdp1::commands
