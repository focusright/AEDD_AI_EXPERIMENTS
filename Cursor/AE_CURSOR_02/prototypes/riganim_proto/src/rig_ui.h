#pragma once

#include <filesystem>

#include "rig_renderer.h"
#include "rig_types.h"

namespace aerigp1 {

struct AppState {
    RigDocument doc;
    RigRenderer renderer;
    HWND hwnd{};
    float cam_yaw{0.6f};
    float cam_pitch{0.35f};
    float cam_dist{3.5f};
    DirectX::XMFLOAT3 cam_target{0.f, 1.f, 0.f};
    bool orbit_drag{false};
    int viewport_w{800};
    int viewport_h{600};
    char joint_name_buf[64]{};
    int reparent_target{-1};
    std::filesystem::path data_dir;
};

void RigUiInit(AppState& app);
void RigUiFrame(AppState& app, float dt);
void RigUiUpdateDocument(AppState& app);

} // namespace aerigp1
