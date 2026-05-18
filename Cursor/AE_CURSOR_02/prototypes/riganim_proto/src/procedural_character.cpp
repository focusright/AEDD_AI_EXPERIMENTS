#include "procedural_character.h"

#include <cmath>

#include "skeleton.h"
#include "skinning.h"

namespace aerigp1 {
namespace {
using namespace DirectX;

static void AddBox(Mesh& mesh, XMFLOAT3 center, XMFLOAT3 half_extents, XMFLOAT4 col, std::uint32_t base_v) {
    const float hx = half_extents.x, hy = half_extents.y, hz = half_extents.z;
    const XMFLOAT3 c[8] = {
        {center.x - hx, center.y - hy, center.z - hz}, {center.x + hx, center.y - hy, center.z - hz},
        {center.x + hx, center.y + hy, center.z - hz}, {center.x - hx, center.y + hy, center.z - hz},
        {center.x - hx, center.y - hy, center.z + hz}, {center.x + hx, center.y - hy, center.z + hz},
        {center.x + hx, center.y + hy, center.z + hz}, {center.x - hx, center.y + hy, center.z + hz},
    };
    const int faces[12][3] = {{0, 1, 2}, {0, 2, 3}, {4, 6, 5}, {4, 7, 6}, {0, 4, 5}, {0, 5, 1},
                              {2, 6, 7}, {2, 7, 3}, {0, 3, 7}, {0, 7, 4}, {1, 5, 6}, {1, 6, 2}};
    const std::uint32_t start = static_cast<std::uint32_t>(mesh.vertices.size());
    for (int i = 0; i < 8; ++i) {
        Vertex v{};
        v.position = c[i];
        v.color = col;
        v.normal = {0.f, 1.f, 0.f};
        mesh.vertices.push_back(v);
    }
    for (int f = 0; f < 12; ++f) {
        mesh.indices.push_back(start + static_cast<std::uint32_t>(faces[f][0]));
        mesh.indices.push_back(start + static_cast<std::uint32_t>(faces[f][1]));
        mesh.indices.push_back(start + static_cast<std::uint32_t>(faces[f][2]));
    }
    (void)base_v;
}

static int AddJointNamed(Skeleton& skel, const char* name, int parent, XMFLOAT3 pos) {
    const int idx = SkeletonAddJoint(skel, name, parent);
    skel.joints[static_cast<std::size_t>(idx)].local_bind.position = pos;
    skel.joints[static_cast<std::size_t>(idx)].local_pose = skel.joints[static_cast<std::size_t>(idx)].local_bind;
    return idx;
}

static void SetJointColor(Skeleton& skel, int idx, std::uint32_t rgb) {
    skel.joints[static_cast<std::size_t>(idx)].display_color = 0xFF000000u | (rgb & 0x00FFFFFFu);
}

} // namespace

void BuildDefaultHumanoid(RigDocument& doc) {
    doc = RigDocument{};
    Mesh& m = doc.mesh;
    Skeleton& sk = doc.skeleton;

    const XMFLOAT4 torso_col{0.55f, 0.58f, 0.65f, 1.f};
    const XMFLOAT4 limb_col{0.50f, 0.52f, 0.58f, 1.f};
    const XMFLOAT4 head_col{0.70f, 0.72f, 0.78f, 1.f};

    AddBox(m, {0.f, 1.05f, 0.f}, {0.22f, 0.30f, 0.14f}, torso_col, 0);
    AddBox(m, {0.f, 1.55f, 0.f}, {0.14f, 0.14f, 0.12f}, head_col, 0);
    AddBox(m, {-0.42f, 1.25f, 0.f}, {0.10f, 0.22f, 0.08f}, limb_col, 0);
    AddBox(m, {-0.62f, 1.05f, 0.f}, {0.08f, 0.18f, 0.07f}, limb_col, 0);
    AddBox(m, {-0.78f, 0.92f, 0.f}, {0.06f, 0.06f, 0.05f}, limb_col, 0);
    AddBox(m, {0.42f, 1.25f, 0.f}, {0.10f, 0.22f, 0.08f}, limb_col, 0);
    AddBox(m, {0.62f, 1.05f, 0.f}, {0.08f, 0.18f, 0.07f}, limb_col, 0);
    AddBox(m, {0.78f, 0.92f, 0.f}, {0.06f, 0.06f, 0.05f}, limb_col, 0);
    AddBox(m, {-0.14f, 0.55f, 0.f}, {0.11f, 0.28f, 0.09f}, limb_col, 0);
    AddBox(m, {-0.14f, 0.22f, 0.f}, {0.09f, 0.22f, 0.08f}, limb_col, 0);
    AddBox(m, {-0.14f, 0.02f, 0.06f}, {0.10f, 0.04f, 0.14f}, limb_col, 0);
    AddBox(m, {0.14f, 0.55f, 0.f}, {0.11f, 0.28f, 0.09f}, limb_col, 0);
    AddBox(m, {0.14f, 0.22f, 0.f}, {0.09f, 0.22f, 0.08f}, limb_col, 0);
    AddBox(m, {0.14f, 0.02f, 0.06f}, {0.10f, 0.04f, 0.14f}, limb_col, 0);

    const int root = AddJointNamed(sk, "root", -1, {0.f, 0.f, 0.f});
    const int pelvis = AddJointNamed(sk, "pelvis", root, {0.f, 0.85f, 0.f});
    const int spine = AddJointNamed(sk, "spine", pelvis, {0.f, 0.20f, 0.f});
    const int chest = AddJointNamed(sk, "chest", spine, {0.f, 0.25f, 0.f});
    const int neck = AddJointNamed(sk, "neck", chest, {0.f, 0.22f, 0.f});
    const int head = AddJointNamed(sk, "head", neck, {0.f, 0.18f, 0.f});
    const int lua = AddJointNamed(sk, "left_upper_arm", chest, {-0.28f, 0.15f, 0.f});
    const int lfa = AddJointNamed(sk, "left_forearm", lua, {-0.22f, 0.f, 0.f});
    const int lhand = AddJointNamed(sk, "left_hand", lfa, {-0.18f, 0.f, 0.f});
    const int rua = AddJointNamed(sk, "right_upper_arm", chest, {0.28f, 0.15f, 0.f});
    const int rfa = AddJointNamed(sk, "right_forearm", rua, {0.22f, 0.f, 0.f});
    const int rhand = AddJointNamed(sk, "right_hand", rfa, {0.18f, 0.f, 0.f});
    const int lth = AddJointNamed(sk, "left_thigh", pelvis, {-0.12f, -0.05f, 0.f});
    const int lca = AddJointNamed(sk, "left_calf", lth, {0.f, -0.38f, 0.f});
    const int lft = AddJointNamed(sk, "left_foot", lca, {0.f, -0.38f, 0.f});
    const int rth = AddJointNamed(sk, "right_thigh", pelvis, {0.12f, -0.05f, 0.f});
    const int rca = AddJointNamed(sk, "right_calf", rth, {0.f, -0.38f, 0.f});
    const int rft = AddJointNamed(sk, "right_foot", rca, {0.f, -0.38f, 0.f});

    SetJointColor(sk, root, 0x888888);
    SetJointColor(sk, pelvis, 0x44AAFF);
    SetJointColor(sk, chest, 0x44FFAA);
    SetJointColor(sk, head, 0xFFCC44);
    SetJointColor(sk, lua, 0xFF6644);
    SetJointColor(sk, rua, 0xFF6644);
    SetJointColor(sk, lth, 0xAA44FF);
    SetJointColor(sk, rth, 0xAA44FF);

    sk.selected = pelvis;
    sk.root_index = root;

    doc.ik_upper_joint = lua;
    doc.ik_lower_joint = lfa;
    doc.ik_end_joint = lhand;

    SkeletonRecomputeInverseBind(sk);
    SkeletonCopyPoseFromBind(sk);
    SkinningAutoWeightNearestJoints(doc, 4);
    SkinningNormalizeAll(doc);

    doc.bind_pose.locals.resize(sk.joints.size());
    for (std::size_t i = 0; i < sk.joints.size(); ++i) {
        doc.bind_pose.locals[i] = sk.joints[i].local_bind;
    }

    BuildSampleWaveClip(doc);
    doc.skinned_vertices = doc.mesh.vertices;
}

void BuildSampleWaveClip(RigDocument& doc) {
    AnimClip clip{};
    clip.name = "sample_wave";
    clip.fps = kAnimFps;
    clip.frame_count = 48;

    auto find_joint = [&](const char* name) -> int {
        for (std::size_t i = 0; i < doc.skeleton.joints.size(); ++i) {
            if (doc.skeleton.joints[i].name == name) {
                return static_cast<int>(i);
            }
        }
        return -1;
    };

    const int rua = find_joint("right_upper_arm");
    const int rfa = find_joint("right_forearm");
    const int pelvis = find_joint("pelvis");
    if (rua < 0) {
        return;
    }

    auto add_track = [&](int ji) {
        AnimTrack tr{};
        tr.joint_index = ji;
        clip.tracks.push_back(tr);
        return static_cast<int>(clip.tracks.size()) - 1;
    };

    const int tr_rua = add_track(rua);
    const int tr_rfa = rfa >= 0 ? add_track(rfa) : -1;
    const int tr_pelvis = pelvis >= 0 ? add_track(pelvis) : -1;

    const Transform bind_rua = doc.skeleton.joints[static_cast<std::size_t>(rua)].local_bind;
    const Transform bind_rfa = rfa >= 0 ? doc.skeleton.joints[static_cast<std::size_t>(rfa)].local_bind : Transform{};
    const Transform bind_pelvis = pelvis >= 0 ? doc.skeleton.joints[static_cast<std::size_t>(pelvis)].local_bind : Transform{};

    for (int f = 0; f <= clip.frame_count; f += 8) {
        const float phase = static_cast<float>(f) / static_cast<float>(clip.frame_count);
        const float wave = std::sin(phase * XM_2PI * 2.f) * 0.5f + 0.5f;

        AnimKey k_rua{};
        k_rua.frame = f;
        k_rua.local = bind_rua;
        EulerDegreesToQuaternion(-30.f - 50.f * wave, 0.f, 20.f * wave, k_rua.local.rotation);
        clip.tracks[static_cast<std::size_t>(tr_rua)].keys.push_back(k_rua);

        if (tr_rfa >= 0) {
            AnimKey k_rfa{};
            k_rfa.frame = f;
            k_rfa.local = bind_rfa;
            EulerDegreesToQuaternion(0.f, 0.f, 35.f * wave, k_rfa.local.rotation);
            clip.tracks[static_cast<std::size_t>(tr_rfa)].keys.push_back(k_rfa);
        }

        if (tr_pelvis >= 0) {
            AnimKey k_p{};
            k_p.frame = f;
            k_p.local = bind_pelvis;
            k_p.local.position.y = bind_pelvis.position.y + std::sin(phase * XM_2PI) * 0.02f;
            clip.tracks[static_cast<std::size_t>(tr_pelvis)].keys.push_back(k_p);
        }
    }

    doc.clips.clear();
    doc.clips.push_back(clip);
    doc.active_clip = 0;
}

} // namespace aerigp1
