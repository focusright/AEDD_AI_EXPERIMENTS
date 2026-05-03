#pragma once

#include <DirectXMath.h>

#include "core/types.h"
#include "gizmo/gizmo_state.h"
#include "viewport/viewport_interaction.h"

namespace aetdp1::gizmo {

struct TranslateGizmo {
    // Screen-space hit test against the three axis segments rooted at `origin_world`.
    GizmoAxis HitTestScreenSpace(const viewport::OrbitCamera& camera, DirectX::FXMVECTOR origin_world, int mouse_x_rel, int mouse_y_rel, int viewport_w,
                                 int viewport_h, float hit_radius_pixels, float axis_length_world) const;

    // Produces a world-space translation delta for the active axis from a mouse pixel delta.
    // `axis_length_world` must match the drawn gizmo arm length (world units) for 1:1 screen motion along the axis.
    DirectX::XMFLOAT3 TranslationDeltaFromMouseDelta(const viewport::OrbitCamera& camera, DirectX::FXMVECTOR origin_world, GizmoAxis axis, int mouse_dx,
                                                    int mouse_dy, int viewport_w, int viewport_h, float axis_length_world) const;
};

} // namespace aetdp1::gizmo
