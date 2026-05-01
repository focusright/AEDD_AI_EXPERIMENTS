#pragma once

struct ImGuiIO;

namespace aetdp1::animation {
struct AnimationDocument;
}
namespace aetdp1::commands {
struct CommandContext;
class CommandHistory;
}
namespace aetdp1::scene {
struct SceneData;
}
namespace aetdp1::selection {
struct SelectionState;
}

namespace aetdp1::timeline {

// Owns only Dear ImGui drawing + transport widgets for the animation document.
// Must not directly implement evaluation math (call `animation_eval` from the caller when time changes).
void DrawTimelinePanel(animation::AnimationDocument& anim, const selection::SelectionState& sel, scene::SceneData& scene, commands::CommandHistory& hist,
                       commands::CommandContext& ctx, const ImGuiIO& io);

} // namespace aetdp1::timeline
