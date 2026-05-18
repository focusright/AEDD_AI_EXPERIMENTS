#include "transform.h"

#include <cmath>
#include <cstdlib>

namespace aerigp1 {
namespace {
using namespace DirectX;
}

XMMATRIX TransformToMatrix(const Transform& t) {
    const XMVECTOR s = XMLoadFloat3(&t.scale);
    const XMVECTOR r = XMLoadFloat4(&t.rotation);
    const XMVECTOR p = XMLoadFloat3(&t.position);
    XMMATRIX m = XMMatrixScalingFromVector(s);
    m = XMMatrixMultiply(m, XMMatrixRotationQuaternion(r));
    m = XMMatrixMultiply(m, XMMatrixTranslationFromVector(p));
    return m;
}

Transform MatrixToTransform(const XMMATRIX& m) {
    Transform out{};
    XMVECTOR s{}, r{}, p{};
    XMMatrixDecompose(&s, &r, &p, m);
    XMStoreFloat3(&out.scale, s);
    XMStoreFloat4(&out.rotation, r);
    XMStoreFloat3(&out.position, p);
    return out;
}

Transform LerpTransform(const Transform& a, const Transform& b, float u) {
    Transform out{};
    const XMVECTOR ta = XMLoadFloat3(&a.position);
    const XMVECTOR tb = XMLoadFloat3(&b.position);
    XMStoreFloat3(&out.position, XMVectorLerp(ta, tb, u));
    const XMVECTOR qa = XMLoadFloat4(&a.rotation);
    const XMVECTOR qb = XMLoadFloat4(&b.rotation);
    XMStoreFloat4(&out.rotation, XMQuaternionNormalize(XMQuaternionSlerp(qa, qb, u)));
    const XMVECTOR sa = XMLoadFloat3(&a.scale);
    const XMVECTOR sb = XMLoadFloat3(&b.scale);
    XMStoreFloat3(&out.scale, XMVectorLerp(sa, sb, u));
    return out;
}

void EulerDegreesToQuaternion(float pitch_deg, float yaw_deg, float roll_deg, XMFLOAT4& out_quat) {
    const float px = XMConvertToRadians(pitch_deg);
    const float py = XMConvertToRadians(yaw_deg);
    const float pz = XMConvertToRadians(roll_deg);
    const XMVECTOR q = XMQuaternionRotationRollPitchYaw(px, py, pz);
    XMStoreFloat4(&out_quat, q);
}

void QuaternionToEulerDegrees(const XMFLOAT4& quat, float& pitch_deg, float& yaw_deg, float& roll_deg) {
    const float x = quat.x, y = quat.y, z = quat.z, w = quat.w;
    const float sinp = 2.f * (w * x - y * z);
    float pitch = 0.f;
    if (std::abs(sinp) >= 1.f) {
        pitch = std::copysign(XM_PIDIV2, sinp);
    } else {
        pitch = std::asin(sinp);
    }
    const float siny_cosp = 2.f * (w * y + x * z);
    const float cosy_cosp = 1.f - 2.f * (x * x + y * y);
    const float yaw = std::atan2(siny_cosp, cosy_cosp);
    const float sinr_cosp = 2.f * (w * z + x * y);
    const float cosr_cosp = 1.f - 2.f * (x * x + z * z);
    const float roll = std::atan2(sinr_cosp, cosr_cosp);
    pitch_deg = XMConvertToDegrees(pitch);
    yaw_deg = XMConvertToDegrees(yaw);
    roll_deg = XMConvertToDegrees(roll);
}

} // namespace aerigp1
