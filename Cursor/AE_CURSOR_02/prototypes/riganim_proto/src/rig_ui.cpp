#include "rig_ui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

#include <imgui.h>
#include <commdlg.h>

#include "animation.h"
#include "ik_two_bone.h"
#include "procedural_character.h"
#include "rig_serializer.h"
#include "rig_validator.h"
#include "skeleton.h"
#include "skinning.h"
#include "transform.h"

namespace aerigp1 {
namespace {
using namespace DirectX;

static std::string WideToUtf8(const wchar_t* w) {
    if (!w) {
        return {};
    }
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n > 0 ? n - 1 : 0), '\0');
    if (n > 0) {
        WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    }
    return s;
}

static bool PickOpenPath(HWND hwnd, std::string& out_path, const wchar_t* filter) {
    wchar_t buf[MAX_PATH]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) {
        return false;
    }
    out_path = WideToUtf8(buf);
    return true;
}

static bool PickSavePath(HWND hwnd, std::string& out_path, const wchar_t* filter) {
    wchar_t buf[MAX_PATH]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameW(&ofn)) {
        return false;
    }
    out_path = WideToUtf8(buf);
    return true;
}

static void DrawHierarchy(AppState& app) {
    RigDocument& doc = app.doc;
  Skeleton& sk = doc.skeleton;
    for (std::size_t i = 0; i < sk.joints.size(); ++i) {
        const bool sel = sk.selected == static_cast<int>(i);
        if (ImGui::Selectable((sk.joints[i].name + "##j" + std::to_string(i)).c_str(), sel)) {
            sk.selected = static_cast<int>(i);
            snprintf(app.joint_name_buf, sizeof(app.joint_name_buf), "%s", sk.joints[i].name.c_str());
        }
    }
}

static void DrawJointInspector(AppState& app) {
    RigDocument& doc = app.doc;
    Skeleton& sk = doc.skeleton;
    if (sk.selected < 0 || sk.selected >= static_cast<int>(sk.joints.size())) {
        ImGui::TextUnformatted("No joint selected.");
        return;
    }
    Joint& j = sk.joints[static_cast<std::size_t>(sk.selected)];
    ImGui::InputText("Name", app.joint_name_buf, sizeof(app.joint_name_buf));
    if (ImGui::Button("Apply name")) {
        j.name = app.joint_name_buf;
    }
    if (ImGui::DragFloat3("Position", &j.local_pose.position.x, 0.01f)) {
        SkeletonUpdateWorldTransforms(sk, true);
    }
    float euler_deg[3]{};
    QuaternionToEulerDegrees(j.local_pose.rotation, euler_deg[0], euler_deg[1], euler_deg[2]);
    if (ImGui::DragFloat3("Rotation (deg)", euler_deg, 1.f)) {
        EulerDegreesToQuaternion(euler_deg[0], euler_deg[1], euler_deg[2], j.local_pose.rotation);
        SkeletonUpdateWorldTransforms(sk, true);
    }
    if (ImGui::DragFloat3("Scale", &j.local_pose.scale.x, 0.01f, 0.01f, 10.f)) {
        SkeletonUpdateWorldTransforms(sk, true);
    }
    ImGui::Separator();
    if (ImGui::Button("Add child joint")) {
        const int ni = SkeletonAddJoint(sk, "new_joint", sk.selected);
        sk.selected = ni;
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete joint")) {
        SkeletonDeleteJoint(sk, sk.selected);
    }
    ImGui::InputInt("Reparent to index", &app.reparent_target);
    ImGui::SameLine();
    if (ImGui::Button("Reparent")) {
        SkeletonReparentJoint(sk, sk.selected, app.reparent_target);
    }
    if (ImGui::Button("Reset pose to bind")) {
        SkeletonCopyPoseFromBind(sk);
    }
    ImGui::SameLine();
    if (ImGui::Button("Recompute bind / inv bind")) {
        SkeletonApplyBindAsPose(sk);
        doc.bind_pose.locals.resize(sk.joints.size());
        for (std::size_t i = 0; i < sk.joints.size(); ++i) {
            doc.bind_pose.locals[i] = sk.joints[i].local_bind;
        }
    }
}

} // namespace

void RigUiInit(AppState& app) {
    BuildDefaultHumanoid(app.doc);
    if (app.doc.skeleton.selected >= 0) {
        snprintf(app.joint_name_buf, sizeof(app.joint_name_buf), "%s", app.doc.skeleton.joints[static_cast<std::size_t>(app.doc.skeleton.selected)].name.c_str());
    }
    if (!app.data_dir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(app.data_dir, ec);
        const auto rig_path = app.data_dir / "sample_character.aerig";
        const auto anim_path = app.data_dir / "sample_wave.aeanim";
        if (!std::filesystem::exists(rig_path)) {
            RigSerializer::SaveRig(app.doc, rig_path.string());
        }
        if (!std::filesystem::exists(anim_path)) {
            RigSerializer::SaveAnim(app.doc, app.doc.active_clip, anim_path.string());
        }
    }
    RigUiUpdateDocument(app);
}

void RigUiUpdateDocument(AppState& app) {
    RigDocument& doc = app.doc;
    if (doc.mode == EditorMode::Animate && !doc.clips.empty() && (doc.playing || doc.current_frame > 0)) {
        AnimationEvaluateClip(doc.clips[static_cast<std::size_t>(doc.active_clip)], doc.current_frame, doc.skeleton);
    }
    if (doc.ik_enabled && doc.mode == EditorMode::Rig) {
        IkSolveTwoBone(doc.skeleton, doc.ik_upper_joint, doc.ik_lower_joint, doc.ik_end_joint, doc.ik_target);
    }
    const int hl = (doc.mode == EditorMode::Skin) ? doc.skeleton.selected : -1;
    SkinningCpuDeform(doc, hl);
    doc.validation_messages = RigValidator::Validate(doc);
}

static void DrawToolsPanel(AppState& app, float dt) {
    RigDocument& doc = app.doc;
    AnimationTickPlayback(doc, dt);
    if (ImGui::RadioButton("Rig", doc.mode == EditorMode::Rig)) {
        doc.mode = EditorMode::Rig;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Skin", doc.mode == EditorMode::Skin)) {
        doc.mode = EditorMode::Skin;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Animate", doc.mode == EditorMode::Animate)) {
        doc.mode = EditorMode::Animate;
    }

    if (doc.mode == EditorMode::Rig) {
        ImGui::Separator();
        DrawHierarchy(app);
        ImGui::Separator();
        DrawJointInspector(app);
        ImGui::Separator();
        ImGui::Checkbox("2-bone IK (left arm)", &doc.ik_enabled);
        ImGui::DragFloat3("IK target", &doc.ik_target.x, 0.02f);
    } else if (doc.mode == EditorMode::Skin) {
        ImGui::Checkbox("Weight debug colors", &doc.show_weight_debug);
        if (ImGui::Button("Auto-weight (nearest joints)")) {
            SkinningAutoWeightNearestJoints(doc, 4);
        }
        ImGui::SameLine();
        if (ImGui::Button("Normalize all weights")) {
            SkinningNormalizeAll(doc);
        }
        DrawHierarchy(app);
    } else {
        if (doc.clips.empty()) {
            ImGui::TextUnformatted("No animation clips.");
        } else {
            AnimClip& clip = doc.clips[static_cast<std::size_t>(doc.active_clip)];
            ImGui::Text("Clip: %s", clip.name.c_str());
            ImGui::SliderInt("Frame", &doc.current_frame, 0, clip.frame_count);
            if (ImGui::Button(doc.playing ? "Pause" : "Play")) {
                doc.playing = !doc.playing;
            }
            ImGui::SameLine();
            if (ImGui::Button("Set keyframe")) {
                AnimationSetKeyframe(doc, doc.skeleton.selected, doc.current_frame);
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete keyframe")) {
                AnimationDeleteKeyframe(doc, doc.skeleton.selected, doc.current_frame);
            }
            DrawHierarchy(app);
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Save rig (.aerig)")) {
        std::string path;
        if (PickSavePath(app.hwnd, path, L"AERIG\0*.aerig\0All\0*.*\0\0")) {
            doc.last_status = RigSerializer::SaveRig(doc, path) ? "Saved rig." : "Save rig failed.";
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load rig")) {
        std::string path;
        if (PickOpenPath(app.hwnd, path, L"AERIG\0*.aerig\0All\0*.*\0\0")) {
            const LoadResult lr = RigSerializer::LoadRig(doc, path);
            doc.last_status = lr.ok ? "Loaded rig." : lr.error;
        }
    }
    if (ImGui::Button("Save anim (.aeanim)")) {
        std::string path;
        if (PickSavePath(app.hwnd, path, L"AEANIM\0*.aeanim\0All\0*.*\0\0")) {
            doc.last_status = RigSerializer::SaveAnim(doc, doc.active_clip, path) ? "Saved anim." : "Save anim failed.";
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load anim")) {
        std::string path;
        if (PickOpenPath(app.hwnd, path, L"AEANIM\0*.aeanim\0All\0*.*\0\0")) {
            const LoadResult lr = RigSerializer::LoadAnim(doc, path);
            doc.last_status = lr.ok ? "Loaded anim." : lr.error;
        }
    }
    if (ImGui::Button("Load bundled samples")) {
        const auto rig_path = (app.data_dir / "sample_character.aerig").string();
        const auto anim_path = (app.data_dir / "sample_wave.aeanim").string();
        const LoadResult lr = RigSerializer::LoadRig(doc, rig_path);
        if (lr.ok) {
            RigSerializer::LoadAnim(doc, anim_path);
            doc.last_status = "Loaded bundled samples.";
        } else {
            BuildDefaultHumanoid(doc);
            doc.last_status = "Regenerated default (bundled files missing).";
        }
    }

    ImGui::SeparatorText("Validation");
    for (const std::string& m : doc.validation_messages) {
        ImGui::BulletText("%s", m.c_str());
    }
    if (!doc.last_status.empty()) {
        ImGui::TextWrapped("%s", doc.last_status.c_str());
    }
}

static void DrawViewportPanel(AppState& app) {
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const int w = std::max(4, static_cast<int>(std::floor(avail.x)));
    const int h = std::max(4, static_cast<int>(std::floor(avail.y)));
    app.viewport_w = w;
    app.viewport_h = h;
    app.renderer.ResizeViewportTexture(static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h));
    ImGui::Image(app.renderer.ViewportSrvGpuHandle(), ImVec2(static_cast<float>(w), static_cast<float>(h)));
    if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        const ImVec2 d = ImGui::GetIO().MouseDelta;
        app.cam_yaw += d.x * 0.01f;
        app.cam_pitch += d.y * 0.01f;
        app.cam_pitch = std::clamp(app.cam_pitch, -1.4f, 1.4f);
    }
    if (ImGui::IsItemHovered()) {
        app.cam_dist -= ImGui::GetIO().MouseWheel * 0.15f;
        app.cam_dist = std::clamp(app.cam_dist, 1.f, 12.f);
    }
}

void RigUiFrame(AppState& app, float dt) {
    const ImGuiViewport* main_vp = ImGui::GetMainViewport();
    const ImVec2 origin = main_vp->WorkPos;
    const ImVec2 size = main_vp->WorkSize;
    const float pad = 8.f;
    const float tools_w = std::max(340.f, size.x * 0.24f);
    const float content_h = size.y - pad * 2.f;
    const float viewport_w = std::max(480.f, size.x - tools_w - pad * 3.f);
    const ImGuiWindowFlags panel_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

    ImGui::SetNextWindowPos(ImVec2(origin.x + pad, origin.y + pad), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(tools_w, content_h), ImGuiCond_Always);
    ImGui::Begin("AERIGP1 — Rig / Skin / Animate", nullptr, panel_flags);
    DrawToolsPanel(app, dt);
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(origin.x + pad * 2.f + tools_w, origin.y + pad), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport_w, content_h), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::Begin("Viewport", nullptr, panel_flags);
    DrawViewportPanel(app);
    ImGui::End();
    ImGui::PopStyleVar();

    RigUiUpdateDocument(app);
    static int validation_log_cooldown = 0;
    if (validation_log_cooldown <= 0) {
        RigValidator::PrintToDebug(app.doc.validation_messages);
        validation_log_cooldown = 120;
    } else {
        --validation_log_cooldown;
    }
}

} // namespace aerigp1
