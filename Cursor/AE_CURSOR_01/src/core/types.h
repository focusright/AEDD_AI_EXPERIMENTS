#pragma once

#include <cstdint>
#include <string>

#include <DirectXMath.h>

namespace aetdp1 {

using ObjectId = std::uint32_t;

constexpr ObjectId kInvalidObjectId = 0;

struct Transform {
    DirectX::XMFLOAT3 translation{0.f, 0.f, 0.f};
    DirectX::XMFLOAT3 scale{1.f, 1.f, 1.f};
    // Quaternion (x, y, z, w). Identity is (0,0,0,1).
    DirectX::XMFLOAT4 rotation{0.f, 0.f, 0.f, 1.f};
};

inline DirectX::XMMATRIX TransformToMatrix(const Transform& t) {
    using namespace DirectX;
    const XMVECTOR q = XMLoadFloat4(&t.rotation);
    const XMMATRIX r = XMMatrixRotationQuaternion(q);
    const XMMATRIX s = XMMatrixScaling(t.scale.x, t.scale.y, t.scale.z);
    const XMMATRIX tr = XMMatrixTranslation(t.translation.x, t.translation.y, t.translation.z);
    return s * r * tr;
}

} // namespace aetdp1
