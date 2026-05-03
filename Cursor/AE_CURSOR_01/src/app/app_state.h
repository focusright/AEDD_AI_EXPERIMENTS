#pragma once

#include <Windows.h>

#include "animation/animation_data.h"
#include "core/types.h"
#include "commands/command_undo.h"
#include "editor/editor_state.h"
#include "gizmo/gizmo_state.h"
#include "gizmo/translate_gizmo.h"
#include "renderer/d3d12_renderer.h"
#include "scene/scene_data.h"
#include "selection/selection_state.h"
#include "viewport/viewport_interaction.h"

#include <imgui.h>

namespace aetdp1::app {

struct ViewportUiState {
    bool hovered{false};
    ImVec2 rect_min{0, 0};
    ImVec2 rect_max{0, 0};
    int w{1};
    int h{1};
};

struct GizmoDragState {
    bool active{false};
    ObjectId object{kInvalidObjectId};
    Transform transform_at_press{};
    int axis{0}; // 1 X, 2 Y, 3 Z
    DirectX::XMFLOAT3 gizmo_origin_press{};
};

struct AppState {
    HWND hwnd{nullptr};

    renderer::D3d12Renderer renderer{};
    scene::SceneData scene{};
    selection::SelectionState selection{};
    editor::EditorState editor{};
    viewport::ViewportInteraction viewport{};
    gizmo::GizmoState gizmo{};
    gizmo::TranslateGizmo translate_gizmo{};
    animation::AnimationDocument anim{};
    commands::CommandHistory history{};
    commands::CommandContext cmd_ctx{};

    ViewportUiState viewport_ui{};
    GizmoDragState gizmo_drag{};

    ObjectId inspector_baseline_object{kInvalidObjectId};
    Transform inspector_baseline_transform{};
    scene::BoxCorners inspector_baseline_box{};
    scene::FaceColors inspector_baseline_face_colors{};
    int inspector_vertex_index_cache{-1};
    bool startup_layout_applied{false};

    // Timeline key list: which key is selected for delete (must match `selection.active_object` track).
    ObjectId timeline_selected_key_object{kInvalidObjectId};
    float timeline_selected_key_time{-1.f};

    std::wstring pending_load_path{};
    std::wstring pending_save_path{};
};

} // namespace aetdp1::app
