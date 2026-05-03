#pragma once

struct ImGuiIO;

namespace aetdp1::app {
struct AppState;
}
namespace aetdp1::commands {
struct CommandContext;
class CommandHistory;
}

namespace aetdp1::timeline {

// Owns only Dear ImGui drawing + transport widgets for the animation document.
// Must not directly implement evaluation math (call `animation_eval` from the caller when time changes).
void DrawTimelinePanel(app::AppState& app, commands::CommandHistory& hist, commands::CommandContext& ctx, const ImGuiIO& io);

} // namespace aetdp1::timeline
