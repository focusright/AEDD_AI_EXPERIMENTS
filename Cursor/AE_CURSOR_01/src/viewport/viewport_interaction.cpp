#include "viewport_interaction.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <DirectXMath.h>

namespace aetdp1::viewport {
namespace {

using namespace DirectX;

static XMVECTOR Load3(const XMFLOAT3& v) { return XMLoadFloat3(&v); }

static bool RayIntersectsAabb(FXMVECTOR ro, FXMVECTOR rd, FXMVECTOR bmin, FXMVECTOR bmax, float& out_t) {
    // Slab method.
    float tmin = 0.f;
    float tmax = std::numeric_limits<float>::infinity();

    for (int axis = 0; axis < 3; ++axis) {
        const float o = XMVectorGetByIndex(ro, axis);
        const float d = XMVectorGetByIndex(rd, axis);
        const float mn = XMVectorGetByIndex(bmin, axis);
        const float mx = XMVectorGetByIndex(bmax, axis);

        if (std::fabs(d) < 1e-8f) {
            if (o < mn || o > mx) {
                return false;
            }
            continue;
        }

        const float inv_d = 1.f / d;
        float t0 = (mn - o) * inv_d;
        float t1 = (mx - o) * inv_d;
        if (t0 > t1) {
            std::swap(t0, t1);
        }
        tmin = std::max(tmin, t0);
        tmax = std::min(tmax, t1);
        if (tmax < tmin) {
            return false;
        }
    }

    out_t = tmin;
    return out_t == out_t && std::isfinite(out_t);
}

static void WorldAabbForObject(const scene::ObjectData& obj, XMVECTOR* out_min, XMVECTOR* out_max) {
    const XMMATRIX m = aetdp1::TransformToMatrix(obj.transform);

    XMVECTOR vmin = XMVectorReplicate(std::numeric_limits<float>::infinity());
    XMVECTOR vmax = XMVectorReplicate(-std::numeric_limits<float>::infinity());
    for (int i = 0; i < 8; ++i) {
        const XMVECTOR p = XMVector3Transform(Load3(obj.local_box.v[i]), m);
        vmin = XMVectorMin(vmin, p);
        vmax = XMVectorMax(vmax, p);
    }
    *out_min = vmin;
    *out_max = vmax;
}

} // namespace

XMMATRIX OrbitCamera::View() const {
    using namespace DirectX;
    const float cos_pitch = std::cosf(pitch_radians);
    const XMVECTOR eye_offset = XMVectorSet(std::cosf(yaw_radians) * cos_pitch * distance, std::sinf(pitch_radians) * distance,
                                            std::sinf(yaw_radians) * cos_pitch * distance, 0.f);
    const XMVECTOR at = XMLoadFloat3(&target);
    const XMVECTOR eye = XMVectorAdd(at, eye_offset);
    return XMMatrixLookAtLH(eye, at, XMVectorSet(0.f, 1.f, 0.f, 0.f));
}

XMMATRIX OrbitCamera::Projection(float aspect_w_over_h) const {
    using namespace DirectX;
    return XMMatrixPerspectiveFovLH(fov_y_radians, aspect_w_over_h, 0.1f, 200.f);
}

void OrbitCamera::ScreenPointToWorldRay(int px_rel, int py_rel, int viewport_w, int viewport_h, XMVECTOR* out_origin, XMVECTOR* out_dir) const {
    using namespace DirectX;
    const float nx = (static_cast<float>(px_rel) + 0.5f) / static_cast<float>(viewport_w) * 2.f - 1.f;
    const float ny = -(static_cast<float>(py_rel) + 0.5f) / static_cast<float>(viewport_h) * 2.f + 1.f;

    const float aspect = static_cast<float>(viewport_w) / static_cast<float>(viewport_h);
    const XMMATRIX inv_vp = XMMatrixInverse(nullptr, View() * Projection(aspect));

    const XMVECTOR p0 = XMVector3TransformCoord(XMVectorSet(nx, ny, 0.f, 1.f), inv_vp);
    const XMVECTOR p1 = XMVector3TransformCoord(XMVectorSet(nx, ny, 1.f, 1.f), inv_vp);
    const XMVECTOR dir = XMVector3Normalize(XMVectorSubtract(p1, p0));
    *out_origin = p0;
    *out_dir = dir;
}

void ViewportInteraction::OrbitFromMouseDelta(float dx_pixels, float dy_pixels) {
    camera.yaw_radians += dx_pixels * 0.005f;
    camera.pitch_radians += dy_pixels * 0.005f;
    const float limit = DirectX::XM_PIDIV2 * 0.99f;
    camera.pitch_radians = std::clamp(camera.pitch_radians, -limit, limit);
}

PickResult ViewportInteraction::PickObject(const scene::SceneData& scene, FXMVECTOR ray_origin, FXMVECTOR ray_dir) const {
    PickResult best{};
    best.intent = PickIntent::None;
    best.object = kInvalidObjectId;

    float best_t = std::numeric_limits<float>::infinity();
    for (const auto& obj : scene.objects) {
        XMVECTOR bmin, bmax;
        WorldAabbForObject(obj, &bmin, &bmax);
        float t = 0.f;
        if (!RayIntersectsAabb(ray_origin, ray_dir, bmin, bmax, t)) {
            continue;
        }
        if (t < best_t) {
            best_t = t;
            best.intent = PickIntent::Object;
            best.object = obj.id;
        }
    }
    return best;
}

} // namespace aetdp1::viewport
