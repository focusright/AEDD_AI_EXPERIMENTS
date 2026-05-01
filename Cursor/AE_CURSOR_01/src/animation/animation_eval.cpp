#include "animation_eval.h"

#include <algorithm>
#include <cmath>

#include <DirectXMath.h>

namespace aetdp1::animation {
namespace {

using namespace DirectX;

static XMVECTOR Load3(const XMFLOAT3& v) { return XMLoadFloat3(&v); }
static void Store3(XMFLOAT3& out, FXMVECTOR v) { XMStoreFloat3(&out, v); }

static XMVECTOR LoadQ(const XMFLOAT4& q) { return XMLoadFloat4(&q); }
static void StoreQ(XMFLOAT4& out, FXMVECTOR q) { XMStoreFloat4(&out, q); }

static bool FindBracket(const TransformTrack& tr, float t, int& out_before, int& out_after) {
    if (tr.keys.empty()) {
        return false;
    }
    if (t <= tr.keys.front().time_seconds) {
        out_before = out_after = 0;
        return true;
    }
    if (t >= tr.keys.back().time_seconds) {
        out_before = out_after = static_cast<int>(tr.keys.size()) - 1;
        return true;
    }
    for (int i = 0; i + 1 < static_cast<int>(tr.keys.size()); ++i) {
        const float t0 = tr.keys[static_cast<std::size_t>(i)].time_seconds;
        const float t1 = tr.keys[static_cast<std::size_t>(i + 1)].time_seconds;
        if (t0 <= t && t <= t1) {
            out_before = i;
            out_after = i + 1;
            return true;
        }
    }
    out_before = out_after = static_cast<int>(tr.keys.size()) - 1;
    return true;
}

static Transform EvalKey(const TransformKeyframe& k) { return k.value; }

static Transform LerpTransform(const Transform& a, const Transform& b, float u) {
    Transform out{};
    const XMVECTOR ta = Load3(a.translation);
    const XMVECTOR tb = Load3(b.translation);
    Store3(out.translation, XMVectorLerp(ta, tb, u));

    const XMVECTOR qa = LoadQ(a.rotation);
    const XMVECTOR qb = LoadQ(b.rotation);
    StoreQ(out.rotation, XMQuaternionNormalize(XMQuaternionSlerp(qa, qb, u)));

    const XMVECTOR sa = Load3(a.scale);
    const XMVECTOR sb = Load3(b.scale);
    Store3(out.scale, XMVectorLerp(sa, sb, u));
    return out;
}

} // namespace

void EvaluateTransformTracksAtTime(const AnimationDocument& doc, scene::SceneData& scene, float time_seconds) {
    for (const TransformTrack& tr : doc.tracks) {
        scene::ObjectData* obj = scene.TryGet(tr.target);
        if (!obj) {
            continue;
        }

        if (tr.keys.empty()) {
            continue;
        }

        int ib = 0, ia = 0;
        if (!FindBracket(tr, time_seconds, ib, ia)) {
            continue;
        }

        const TransformKeyframe& kb = tr.keys[static_cast<std::size_t>(ib)];
        const TransformKeyframe& ka = tr.keys[static_cast<std::size_t>(ia)];

        Transform evaluated{};
        if (ib == ia) {
            evaluated = EvalKey(kb);
        } else {
            const float t0 = kb.time_seconds;
            const float t1 = ka.time_seconds;
            const float denom = (t1 - t0);
            const float u = denom > 1e-6f ? (time_seconds - t0) / denom : 0.f;
            evaluated = LerpTransform(kb.value, ka.value, std::clamp(u, 0.f, 1.f));
        }

        obj->transform = evaluated;
    }
}

} // namespace aetdp1::animation
