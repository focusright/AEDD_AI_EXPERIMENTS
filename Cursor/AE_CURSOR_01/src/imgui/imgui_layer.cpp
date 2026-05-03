#include "imgui/imgui_layer.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include <commdlg.h>

#include <imgui.h>

#include "animation/animation_eval.h"
#include "app/app_state.h"
#include "commands/command_undo.h"
#include "core/types.h"
#include "editor/editor_state.h"
#include "gizmo/gizmo_state.h"
#include "modeling/modeling_commands.h"
#include "scene/scene_data.h"
#include "viewport/viewport_interaction.h"
#include "serialization/serialization.h"
#include "timeline/timeline_ui.h"

#pragma comment(lib, "comdlg32.lib")

namespace aetdp1::imgui_layer {
namespace {

using namespace DirectX;

static void RefreshInspectorBaseline(app::AppState& app) {
    const ObjectId id = app.selection.active_object;
    if (id != app.inspector_baseline_object) {
        app.inspector_baseline_object = id;
        if (scene::ObjectData* o = app.scene.TryGet(id)) {
            app.inspector_baseline_transform = o->transform;
            app.inspector_baseline_box = o->local_box;
            app.inspector_baseline_face_colors = o->face_colors;
            app.inspector_vertex_index_cache = static_cast<int>(o->selected_vertex_index);
        } else {
            app.inspector_baseline_transform = {};
            app.inspector_baseline_box = {};
            app.inspector_baseline_face_colors = {};
            app.inspector_vertex_index_cache = -1;
        }
    }
}

static std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) {
        return {};
    }
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), out.data(), n, nullptr, nullptr);
    return out;
}

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

static bool PickFileOpen(HWND owner, std::wstring& out_path) {
    wchar_t file[1024]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = file;
    ofn.nMaxFile = static_cast<DWORD>(std::size(file));
    ofn.lpstrFilter = L"AETDP1 text\0*.aetdp1txt\0All files\0*.*\0\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) {
        return false;
    }
    out_path = file;
    return true;
}

static bool PickFileSave(HWND owner, std::wstring& out_path) {
    wchar_t file[1024]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = file;
    ofn.nMaxFile = static_cast<DWORD>(std::size(file));
    ofn.lpstrFilter = L"AETDP1 text\0*.aetdp1txt\0All files\0*.*\0\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameW(&ofn)) {
        return false;
    }
    out_path = file;
    return true;
}

static void DrawMainMenu(app::AppState& app) {
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Save...")) {
            std::wstring path;
            if (PickFileSave(app.hwnd, path)) {
                const std::string utf8 = WideToUtf8(path);
                const std::string data = serialization::SaveTextDocument(app.scene, app.anim);
                std::ofstream out(utf8, std::ios::binary);
                out.write(data.data(), static_cast<std::streamsize>(data.size()));
            }
        }
        if (ImGui::MenuItem("Load...")) {
            std::wstring path;
            if (PickFileOpen(app.hwnd, path)) {
                const std::string utf8 = WideToUtf8(path);
                std::ifstream in(utf8, std::ios::binary);
                std::ostringstream oss;
                oss << in.rdbuf();
                const auto loaded = serialization::LoadTextDocument(oss.str(), app.scene, app.anim);
                if (!loaded.ok) {
                    MessageBoxW(app.hwnd, Utf8ToWide(loaded.error).c_str(), L"AETDP1 load failed", MB_ICONERROR);
                } else {
                    app.selection.Clear();
                    app.inspector_baseline_object = kInvalidObjectId;
                    animation::EvaluateTransformTracksAtTime(app.anim, app.scene, app.anim.current_time_seconds);
                }
            }
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, app.history.CanUndo())) {
            app.history.Undo(app.cmd_ctx);
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, app.history.CanRedo())) {
            app.history.Redo(app.cmd_ctx);
        }
        ImGui::EndMenu();
    }
}

static void DrawModeToolbar(app::AppState& app) {
    ImGui::Begin("Tool mode", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    if (ImGui::RadioButton("Modeling", app.editor.tool_mode == editor::EditorToolMode::Modeling)) {
        app.editor.tool_mode = editor::EditorToolMode::Modeling;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Animation", app.editor.tool_mode == editor::EditorToolMode::Animation)) {
        app.editor.tool_mode = editor::EditorToolMode::Animation;
    }
    ImGui::Checkbox("Selection highlight", &app.editor.show_selection_highlight);
    ImGui::End();
}

static void DrawOutliner(app::AppState& app) {
    ImGui::Begin("Outliner", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    if (ImGui::Button("Create box")) {
        scene::ObjectData created = commands::MakeDefaultNamedObject(app.scene, "Box");
        app.history.Execute(commands::CommandHistory::MakeCreateObject(std::move(created)), app.cmd_ctx);
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate")) {
        if (app.selection.active_object != kInvalidObjectId) {
            app.history.Execute(commands::CommandHistory::MakeDuplicateObject(app.selection.active_object), app.cmd_ctx);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        if (app.selection.active_object != kInvalidObjectId) {
            scene::ObjectData* o = app.scene.TryGet(app.selection.active_object);
            if (o) {
                const ObjectId id = o->id;
                scene::ObjectData snap = *o;
                selection::SelectionState sel_snap = app.selection;
                app.history.Execute(commands::CommandHistory::MakeDeleteObject(id, std::move(snap), std::move(sel_snap)), app.cmd_ctx);
            }
        }
    }

    ImGui::Separator();
    for (const auto& obj : app.scene.objects) {
        const bool active = (obj.id == app.selection.active_object);
        if (ImGui::Selectable(obj.name.c_str(), active)) {
            selection::SelectionState before = app.selection;
            selection::SelectionState after = before;
            after.SetSingleSelection(obj.id);
            app.history.Execute(commands::CommandHistory::MakeReplaceSelection(std::move(before), std::move(after)), app.cmd_ctx);
        }
    }
    ImGui::End();
}

static void DrawInspector(app::AppState& app) {
    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    scene::ObjectData* o = app.scene.TryGet(app.selection.active_object);
    if (!o) {
        ImGui::TextDisabled("No active object.");
        ImGui::End();
        return;
    }

    char name_buf[256]{};
    std::snprintf(name_buf, sizeof(name_buf), "%s", o->name.c_str());
    if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
        o->name = name_buf;
    }

    ImGui::DragFloat3("Translation", &o->transform.translation.x, 0.01f);
    ImGui::DragFloat3("Scale", &o->transform.scale.x, 0.01f, 0.01f, 50.f);
    ImGui::DragFloat4("Rotation (qx,qy,qz,qw)", &o->transform.rotation.x, 0.01f);
    {
        XMVECTOR q = XMLoadFloat4(&o->transform.rotation);
        const float len2 = XMVectorGetX(XMVector4LengthSq(q));
        if (len2 > 1e-20f) {
            XMStoreFloat4(&o->transform.rotation, XMQuaternionNormalize(q));
        } else {
            o->transform.rotation = {0.f, 0.f, 0.f, 1.f};
        }
    }
    ImGui::ColorEdit3("Color", &o->color.x);

    if (ImGui::Button("Commit transform (undoable)")) {
        app.history.Execute(commands::CommandHistory::MakeSetTransform(o->id, app.inspector_baseline_transform, o->transform), app.cmd_ctx);
        app.inspector_baseline_transform = o->transform;
    }
    ImGui::SameLine();
    if (ImGui::Button("Revert transform")) {
        o->transform = app.inspector_baseline_transform;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Per-face colors");
    static const char* kFaceLabels[6] = {"Face -Y", "Face +Y", "Face -Z", "Face +Z", "Face -X", "Face +X"};
    for (int i = 0; i < 6; ++i) {
        ImGui::ColorEdit3(kFaceLabels[i], &o->face_colors.rgb[static_cast<std::size_t>(i)].x);
    }
    if (ImGui::Button("Commit face colors (undoable)")) {
        app.history.Execute(commands::CommandHistory::MakeSetFaceColors(o->id, app.inspector_baseline_face_colors, o->face_colors), app.cmd_ctx);
        app.inspector_baseline_face_colors = o->face_colors;
    }
    ImGui::SameLine();
    if (ImGui::Button("Revert face colors")) {
        o->face_colors = app.inspector_baseline_face_colors;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Tiny geometry edit (local vertex)");
    int v = static_cast<int>(o->selected_vertex_index);
    ImGui::SliderInt("Vertex index", &v, 0, 7);
    o->selected_vertex_index = static_cast<std::uint8_t>(v);
    if (app.inspector_vertex_index_cache != v) {
        app.inspector_baseline_box.v[o->selected_vertex_index] = o->local_box.v[o->selected_vertex_index];
        app.inspector_vertex_index_cache = v;
    }
    ImGui::DragFloat3("Local vertex", &o->local_box.v[o->selected_vertex_index].x, 0.01f);
    if (ImGui::Button("Commit vertex (undoable)")) {
        const DirectX::XMFLOAT3 before = app.inspector_baseline_box.v[o->selected_vertex_index];
        const DirectX::XMFLOAT3 after = o->local_box.v[o->selected_vertex_index];
        app.history.Execute(commands::CommandHistory::MakeNudgeVertex(o->id, o->selected_vertex_index, before, after), app.cmd_ctx);
        app.inspector_baseline_box = o->local_box;
    }

    ImGui::End();
}

static void DrawViewport(app::AppState& app) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const int w = std::max(4, static_cast<int>(std::floor(avail.x)));
    const int h = std::max(4, static_cast<int>(std::floor(avail.y)));

    const ImTextureID tex = app.renderer.ViewportSrvGpuHandle();
    ImGui::Image(tex, ImVec2(static_cast<float>(w), static_cast<float>(h)));

    app.viewport_ui.hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    app.viewport_ui.rect_min = ImGui::GetItemRectMin();
    app.viewport_ui.rect_max = ImGui::GetItemRectMax();
    app.viewport_ui.w = w;
    app.viewport_ui.h = h;

    ImGuiIO& io = ImGui::GetIO();

    const bool orbit_gesture = app.viewport_ui.hovered && ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    if (orbit_gesture && !app.gizmo_drag.active && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        app.viewport.input.rmb_down = true;
        app.viewport.OrbitFromMouseDelta(io.MouseDelta.x, io.MouseDelta.y);
    } else {
        app.viewport.input.rmb_down = false;
    }

    const ImVec2 mouse = io.MousePos;
    const int mx = static_cast<int>(std::floor(mouse.x - app.viewport_ui.rect_min.x));
    const int my = static_cast<int>(std::floor(mouse.y - app.viewport_ui.rect_min.y));

    XMVECTOR ro{};
    XMVECTOR rd{};
    app.viewport.camera.ScreenPointToWorldRay(mx, my, w, h, &ro, &rd);

    scene::ObjectData* active_obj = app.scene.TryGet(app.selection.active_object);
    const bool have_active = active_obj != nullptr;
    XMFLOAT3 gizmo_origin{};
    if (have_active) {
        gizmo_origin = active_obj->transform.translation;
    }
    const XMVECTOR origin = XMLoadFloat3(&gizmo_origin);

    if (app.viewport_ui.hovered) {

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && app.editor.tool_mode == editor::EditorToolMode::Modeling) {
            if (have_active) {
                const gizmo::GizmoAxis axis = app.translate_gizmo.HitTestScreenSpace(app.viewport.camera, origin, mx, my, w, h, app.gizmo.hit_radius_pixels,
                                                                                      app.gizmo.axis_length_world);
                const int axis_i = static_cast<int>(axis);
                if (axis_i != 0) {
                    app.gizmo_drag.active = true;
                    app.gizmo_drag.object = active_obj->id;
                    app.gizmo_drag.transform_at_press = active_obj->transform;
                    app.gizmo_drag.axis = axis_i;
                    app.gizmo_drag.gizmo_origin_press = active_obj->transform.translation;
                    app.gizmo.active_axis = axis;
                    app.gizmo.dragging = true;
                } else {
                    const viewport::PickResult pick = app.viewport.PickObject(app.scene, ro, rd);
                    selection::SelectionState before = app.selection;
                    selection::SelectionState after = before;
                    if (pick.intent == viewport::PickIntent::Object && pick.object != kInvalidObjectId) {
                        after.SetSingleSelection(pick.object);
                    } else {
                        after.Clear();
                    }
                    app.history.Execute(commands::CommandHistory::MakeReplaceSelection(std::move(before), std::move(after)), app.cmd_ctx);
                }
            } else {
                const viewport::PickResult pick = app.viewport.PickObject(app.scene, ro, rd);
                selection::SelectionState before = app.selection;
                selection::SelectionState after = before;
                if (pick.intent == viewport::PickIntent::Object && pick.object != kInvalidObjectId) {
                    after.SetSingleSelection(pick.object);
                } else {
                    after.Clear();
                }
                app.history.Execute(commands::CommandHistory::MakeReplaceSelection(std::move(before), std::move(after)), app.cmd_ctx);
            }
        }

        if (have_active) {
            const gizmo::GizmoAxis axis = app.translate_gizmo.HitTestScreenSpace(app.viewport.camera, origin, mx, my, w, h, app.gizmo.hit_radius_pixels,
                                                                                 app.gizmo.axis_length_world);
            app.gizmo.hovered_axis = app.gizmo.dragging ? app.gizmo.active_axis : axis;
        } else {
            app.gizmo.hovered_axis = gizmo::GizmoAxis::None;
        }
    } else if (!app.gizmo.dragging) {
        app.gizmo.hovered_axis = gizmo::GizmoAxis::None;
    }

    // Maintain gizmo capture while LMB is held, even if hover temporarily drops.
    if (app.gizmo_drag.active && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        scene::ObjectData* go = app.scene.TryGet(app.gizmo_drag.object);
        if (go) {
            const XMVECTOR origin_press = XMLoadFloat3(&app.gizmo_drag.gizmo_origin_press);
            const DirectX::XMFLOAT3 d = app.translate_gizmo.TranslationDeltaFromMouseDelta(
                app.viewport.camera, origin_press, static_cast<gizmo::GizmoAxis>(app.gizmo_drag.axis), static_cast<int>(io.MouseDelta.x),
                static_cast<int>(io.MouseDelta.y), w, h);
            go->transform.translation.x += d.x;
            go->transform.translation.y += d.y;
            go->transform.translation.z += d.z;
        }
    }

    if (app.gizmo_drag.active && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        scene::ObjectData* go = app.scene.TryGet(app.gizmo_drag.object);
        if (go) {
            app.history.Execute(commands::CommandHistory::MakeSetTransform(go->id, app.gizmo_drag.transform_at_press, go->transform), app.cmd_ctx);
        }
        app.gizmo_drag.active = false;
        app.gizmo.dragging = false;
        app.gizmo.active_axis = gizmo::GizmoAxis::None;
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace

void BuildFrame(app::AppState& app, float delta_seconds) {
    (void)delta_seconds;

    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        app.history.Undo(app.cmd_ctx);
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
        app.history.Redo(app.cmd_ctx);
    }

    if (ImGui::BeginMainMenuBar()) {
        DrawMainMenu(app);
        ImGui::EndMainMenuBar();
    }

    RefreshInspectorBaseline(app);

    const ImGuiViewport* main_vp = ImGui::GetMainViewport();
    const ImVec2 p = main_vp->WorkPos;
    const ImVec2 s = main_vp->WorkSize;

    const float pad = 8.0f;
    const float menu_h = 24.0f;
    const float toolbar_h = 64.0f;
    const float timeline_h = std::max(180.0f, s.y * 0.26f);
    const float left_w = std::max(220.0f, s.x * 0.20f);
    const float right_w = std::max(280.0f, s.x * 0.24f);

    const float top_y = p.y + pad + menu_h;
    const float content_h = s.y - (pad * 3.0f) - menu_h - toolbar_h - timeline_h;
    const float viewport_w = std::max(360.0f, s.x - (pad * 4.0f) - left_w - right_w);
    const float row_y = top_y + toolbar_h + pad;

    // Apply deterministic layout every frame so panels never drift/overlap.
    ImGui::SetNextWindowPos(ImVec2(p.x + pad, top_y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(s.x - 2.0f * pad, toolbar_h), ImGuiCond_Always);
    DrawModeToolbar(app);

    ImGui::SetNextWindowPos(ImVec2(p.x + pad, row_y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(left_w, content_h), ImGuiCond_Always);
    DrawOutliner(app);

    ImGui::SetNextWindowPos(ImVec2(p.x + 2.0f * pad + left_w, row_y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport_w, content_h), ImGuiCond_Always);
    DrawViewport(app);

    ImGui::SetNextWindowPos(ImVec2(p.x + 3.0f * pad + left_w + viewport_w, row_y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(right_w, content_h), ImGuiCond_Always);
    DrawInspector(app);

    ImGui::SetNextWindowPos(ImVec2(p.x + pad, row_y + content_h + pad), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(s.x - 2.0f * pad, timeline_h), ImGuiCond_Always);
    timeline::DrawTimelinePanel(app, app.history, app.cmd_ctx, io);
}

} // namespace aetdp1::imgui_layer
