#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include "transform.h"

namespace aerigp1 {

static constexpr int kMaxSkinInfluences = 4;
static constexpr float kAnimFps = 24.f;

struct SkinWeight {
    int joint_indices[kMaxSkinInfluences]{-1, -1, -1, -1};
    float weights[kMaxSkinInfluences]{0.f, 0.f, 0.f, 0.f};
};

struct Vertex {
    DirectX::XMFLOAT3 position{};
    DirectX::XMFLOAT3 normal{0.f, 1.f, 0.f};
    DirectX::XMFLOAT4 color{0.7f, 0.7f, 0.75f, 1.f};
    SkinWeight skin{};
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

struct Joint {
    std::string name;
    int parent{-1};
    Transform local_bind{};
    Transform local_pose{};
    DirectX::XMFLOAT4X4 world_bind{};
    DirectX::XMFLOAT4X4 world_pose{};
    DirectX::XMFLOAT4X4 inverse_bind{};
    std::uint32_t display_color{0xFF88CCFF};
};

struct Skeleton {
    std::vector<Joint> joints;
    int selected{-1};
    int root_index{0};
};

struct Pose {
    std::vector<Transform> locals;
};

struct AnimKey {
    int frame{0};
    Transform local{};
};

struct AnimTrack {
    int joint_index{-1};
    std::vector<AnimKey> keys;
};

struct AnimClip {
    std::string name{"clip"};
    float fps{kAnimFps};
    int frame_count{48};
    std::vector<AnimTrack> tracks;
};

enum class EditorMode {
    Rig,
    Skin,
    Animate
};

struct RigDocument {
    Mesh mesh;
    Skeleton skeleton;
    Pose bind_pose;
    std::vector<AnimClip> clips;
    int active_clip{0};
    EditorMode mode{EditorMode::Rig};
    bool playing{false};
    int current_frame{0};
    float playback_accum{0.f};
    bool show_weight_debug{true};
    bool ik_enabled{false};
    int ik_upper_joint{-1};
    int ik_lower_joint{-1};
    int ik_end_joint{-1};
    DirectX::XMFLOAT3 ik_target{0.f, 1.2f, 0.5f};
    std::vector<Vertex> skinned_vertices;
    std::vector<std::string> validation_messages;
    std::string last_status;
};

} // namespace aerigp1
