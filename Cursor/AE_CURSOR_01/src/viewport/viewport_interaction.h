#pragma once

#include <DirectXMath.h>

#include "core/types.h"
#include "scene/scene_data.h"

namespace aetdp1::viewport {

struct OrbitCamera {
    float yaw_radians{0.65f};
    float pitch_radians{0.35f};
    float distance{7.5f};
    DirectX::XMFLOAT3 target{0.f, 0.35f, 0.f};
    float fov_y_radians{DirectX::XM_PIDIV4};

    DirectX::XMMATRIX View() const;
    DirectX::XMMATRIX Projection(float aspect_w_over_h) const;

    // Pixel coordinates relative to the viewport's top-left (inside the viewport).
    void ScreenPointToWorldRay(int px_rel, int py_rel, int viewport_w, int viewport_h, DirectX::XMVECTOR* out_origin, DirectX::XMVECTOR* out_dir) const;
};

enum class PickIntent : int { None = 0, Object = 1, GizmoAxisX = 2, GizmoAxisY = 3, GizmoAxisZ = 4 };

struct PickResult {
    PickIntent intent{PickIntent::None};
    ObjectId object{kInvalidObjectId};
};

struct ViewportInputState {
    bool rmb_down{false};
    bool lmb_down{false};
    bool alt_down{false};

    int last_mouse_x{0};
    int last_mouse_y{0};
};

struct ViewportInteraction {
    OrbitCamera camera{};
    ViewportInputState input{};

    void OrbitFromMouseDelta(float dx_pixels, float dy_pixels);

    // Ray vs object world AABBs built from local box corners.
    PickResult PickObject(const scene::SceneData& scene, DirectX::FXMVECTOR ray_origin, DirectX::FXMVECTOR ray_dir) const;
};

} // namespace aetdp1::viewport
