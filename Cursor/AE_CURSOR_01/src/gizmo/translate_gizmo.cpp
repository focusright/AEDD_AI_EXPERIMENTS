#include "translate_gizmo.h"

#include <algorithm>
#include <cmath>

namespace aetdp1::gizmo {
namespace {

using namespace DirectX;

static XMVECTOR AxisUnit(GizmoAxis a) {
    switch (a) {
    case GizmoAxis::X:
        return XMVectorSet(1.f, 0.f, 0.f, 0.f);
    case GizmoAxis::Y:
        return XMVectorSet(0.f, 1.f, 0.f, 0.f);
    case GizmoAxis::Z:
        return XMVectorSet(0.f, 0.f, 1.f, 0.f);
    default:
        return XMVectorZero();
    }
}

static void WorldToScreen(const viewport::OrbitCamera& camera, FXMVECTOR world, int vp_w, int vp_h, float* out_sx, float* out_sy) {
    const XMMATRIX view = camera.View();
    const float aspect = static_cast<float>(vp_w) / static_cast<float>(vp_h);
    const XMMATRIX proj = camera.Projection(aspect);
    const XMMATRIX vp = view * proj;

    XMVECTOR clip = XMVector3TransformCoord(world, vp);
    const float x = XMVectorGetX(clip);
    const float y = XMVectorGetY(clip);

    *out_sx = (x * 0.5f + 0.5f) * static_cast<float>(vp_w);
    *out_sy = (-y * 0.5f + 0.5f) * static_cast<float>(vp_h);
}

static float DistPointSegment2D(float px, float py, float ax, float ay, float bx, float by) {
    const float abx = bx - ax;
    const float aby = by - ay;
    const float apx = px - ax;
    const float apy = py - ay;
    const float ab2 = abx * abx + aby * aby;
    if (ab2 < 1e-8f) {
        return std::hypot(px - ax, py - ay);
    }
    float t = (apx * abx + apy * aby) / ab2;
    t = std::clamp(t, 0.f, 1.f);
    const float cx = ax + abx * t;
    const float cy = ay + aby * t;
    return std::hypot(px - cx, py - cy);
}

static void CameraBasisXY(const viewport::OrbitCamera& camera, XMVECTOR* out_right, XMVECTOR* out_up) {
    using namespace DirectX;
    const float cos_pitch = std::cosf(camera.pitch_radians);
    const XMVECTOR eye_offset =
        XMVectorSet(std::cosf(camera.yaw_radians) * cos_pitch * camera.distance, std::sinf(camera.pitch_radians) * camera.distance,
                    std::sinf(camera.yaw_radians) * cos_pitch * camera.distance, 0.f);
    const XMVECTOR at = XMLoadFloat3(&camera.target);
    const XMVECTOR eye = XMVectorAdd(at, eye_offset);
    const XMVECTOR forward = XMVector3Normalize(XMVectorSubtract(at, eye));

    XMVECTOR world_up = XMVectorSet(0.f, 1.f, 0.f, 0.f);
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(world_up, forward));
    if (XMVectorGetX(XMVector3LengthSq(right)) < 1e-8f) {
        world_up = XMVectorSet(0.f, 0.f, 1.f, 0.f);
        right = XMVector3Normalize(XMVector3Cross(world_up, forward));
    }
    const XMVECTOR up = XMVector3Normalize(XMVector3Cross(forward, right));
    *out_right = right;
    *out_up = up;
}

} // namespace

GizmoAxis TranslateGizmo::HitTestScreenSpace(const viewport::OrbitCamera& camera, FXMVECTOR origin_world, int mouse_x_rel, int mouse_y_rel, int viewport_w,
                                             int viewport_h, float hit_radius_pixels, float axis_length_world) const {
    const GizmoAxis axes[3] = {GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z};
    float best_d = hit_radius_pixels;
    GizmoAxis best = GizmoAxis::None;

    for (GizmoAxis a : axes) {
        const XMVECTOR axis = AxisUnit(a);
        const XMVECTOR p0 = origin_world;
        const XMVECTOR p1 = XMVectorAdd(origin_world, XMVectorScale(axis, axis_length_world));

        float x0, y0, x1, y1;
        WorldToScreen(camera, p0, viewport_w, viewport_h, &x0, &y0);
        WorldToScreen(camera, p1, viewport_w, viewport_h, &x1, &y1);

        const float d = DistPointSegment2D(static_cast<float>(mouse_x_rel), static_cast<float>(mouse_y_rel), x0, y0, x1, y1);
        if (d < best_d) {
            best_d = d;
            best = a;
        }
    }

    return best;
}

XMFLOAT3 TranslateGizmo::TranslationDeltaFromMouseDelta(const viewport::OrbitCamera& camera, FXMVECTOR origin_world, GizmoAxis axis, int mouse_dx, int mouse_dy,
                                                      int viewport_w, int viewport_h) const {
    (void)viewport_w;
    (void)viewport_h;
    XMVECTOR right{};
    XMVECTOR up{};
    CameraBasisXY(camera, &right, &up);

    const XMVECTOR axis_w = XMVector3Normalize(AxisUnit(axis));

    // Sensitivity scales with distance so dragging feels stable-ish at different zoom levels.
    const XMVECTOR eye_offset = XMVectorSubtract(origin_world, XMLoadFloat3(&camera.target));
    const float dist = std::max(0.5f, XMVectorGetX(XMVector3Length(eye_offset)));
    const float sens = 0.0025f * dist;

    const float du = static_cast<float>(mouse_dx);
    const float dv = static_cast<float>(mouse_dy);
    const XMVECTOR mouse_plane = XMVectorAdd(XMVectorScale(right, du * sens), XMVectorScale(up, -dv * sens));

    const float along = XMVectorGetX(XMVector3Dot(mouse_plane, axis_w));
    const XMVECTOR delta = XMVectorScale(axis_w, along);
    XMFLOAT3 out{};
    XMStoreFloat3(&out, delta);
    return out;
}

} // namespace aetdp1::gizmo
