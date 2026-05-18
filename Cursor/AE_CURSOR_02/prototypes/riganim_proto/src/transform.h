#pragma once

#include <DirectXMath.h>

namespace aerigp1 {

struct Transform {
    DirectX::XMFLOAT3 position{0.f, 0.f, 0.f};
    DirectX::XMFLOAT4 rotation{0.f, 0.f, 0.f, 1.f};
    DirectX::XMFLOAT3 scale{1.f, 1.f, 1.f};
};

DirectX::XMMATRIX TransformToMatrix(const Transform& t);
Transform MatrixToTransform(const DirectX::XMMATRIX& m);
Transform LerpTransform(const Transform& a, const Transform& b, float u);

void EulerDegreesToQuaternion(float pitch_deg, float yaw_deg, float roll_deg, DirectX::XMFLOAT4& out_quat);
void QuaternionToEulerDegrees(const DirectX::XMFLOAT4& quat, float& pitch_deg, float& yaw_deg, float& roll_deg);

} // namespace aerigp1
