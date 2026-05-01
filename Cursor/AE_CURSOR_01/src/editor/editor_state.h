#pragma once

namespace aetdp1::editor {

// High-level editor mode boundary. This prototype keeps the split explicit even though both tools share one window.
enum class EditorToolMode : int { Modeling = 0, Animation = 1 };

struct EditorState {
    EditorToolMode tool_mode{EditorToolMode::Modeling};

    // UI-ish toggles (not scene truth).
    bool show_world_axes{true};
    bool show_selection_highlight{true};
};

} // namespace aetdp1::editor
