#include "timeline/timeline_ui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <imgui.h>

#include "animation/animation_data.h"
#include "app/app_state.h"
#include "core/types.h"
#include "animation/animation_eval.h"
#include "commands/command_undo.h"
#include "modeling/modeling_commands.h"
#include "scene/scene_data.h"
#include "selection/selection_state.h"

namespace aetdp1::timeline {
namespace {

static bool FindKeyframeAtExactTime(const animation::TransformTrack& tr, float t_seconds, animation::TransformKeyframe* out_key) {
    for (const auto& k : tr.keys) {
        if (k.time_seconds == t_seconds) {
            if (out_key) {
                *out_key = k;
            }
            return true;
        }
    }
    return false;
}

static bool TimelineKeyListSelectionValid(const app::AppState& app, ObjectId track_object, const animation::TransformTrack* tr) {
    if (!tr) {
        return false;
    }
    if (app.timeline_selected_key_object != track_object) {
        return false;
    }
    if (app.timeline_selected_key_time < 0.f) {
        return false;
    }
    return FindKeyframeAtExactTime(*tr, app.timeline_selected_key_time, nullptr);
}

static void ClearTimelineKeyListSelection(app::AppState& app) {
    app.timeline_selected_key_object = kInvalidObjectId;
    app.timeline_selected_key_time = -1.f;
}

// If `preferred_t` is already keyed, advance in small steps so "Insert" always adds a new key
// (UpsertKeyframe replaces when times match exactly, which looked like insert only working once).
static float AllocateInsertTimeForNewKey(const animation::AnimationDocument& anim, const animation::TransformTrack* tr, float preferred_t) {
    const float dur = std::max(0.25f, anim.duration_seconds);
    float t = std::clamp(preferred_t, 0.f, dur);
    if (!tr || tr->keys.empty()) {
        return t;
    }
    constexpr float kStep = 1e-4f;
    constexpr int kMaxSteps = 1000000;
    for (int step = 0; step < kMaxSteps; ++step) {
        bool taken = false;
        for (const auto& k : tr->keys) {
            if (k.time_seconds == t) {
                taken = true;
                break;
            }
        }
        if (!taken) {
            return t;
        }
        t += kStep;
        if (t > dur) {
            return -1.f;
        }
    }
    return -1.f;
}

} // namespace

void DrawTimelinePanel(app::AppState& app, commands::CommandHistory& hist, commands::CommandContext& ctx, const ImGuiIO& io) {
    animation::AnimationDocument& anim = app.anim;
    const selection::SelectionState& sel = app.selection;
    scene::SceneData& scene = app.scene;

    ImGui::Begin("Timeline / Animation", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

    ImGui::Text("Duration (s)");
    ImGui::SameLine();
    ImGui::DragFloat("##dur", &anim.duration_seconds, 0.05f, 0.25f, 600.f);

    ImGui::Separator();
    ImGui::Text("Transport");
    if (ImGui::Button("Play")) {
        anim.playback.playing = true;
        anim.playback.paused = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Pause")) {
        anim.playback.paused = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        anim.playback.playing = false;
        anim.playback.paused = false;
        anim.current_time_seconds = 0.f;
        animation::EvaluateTransformTracksAtTime(anim, scene, anim.current_time_seconds);
    }

    ImGui::Separator();
    ImGui::Text("Scrub");
    // While playing, do not bind `current_time_seconds` to SliderFloat: if the scrub slider stays active,
    // SliderBehavior overwrites time from the mouse each frame and cancels the main loop's playback advance.
    if (anim.playback.playing && !anim.playback.paused) {
        ImGui::Text("%.3f s  (pause playback to scrub)", anim.current_time_seconds);
    } else {
        const float before_time = anim.current_time_seconds;
        ImGui::SliderFloat("##time", &anim.current_time_seconds, 0.f, std::max(0.25f, anim.duration_seconds), "%.3f s");
        if (before_time != anim.current_time_seconds) {
            animation::EvaluateTransformTracksAtTime(anim, scene, anim.current_time_seconds);
        }
    }

    ImGui::Separator();
    ImGui::Text("Keyframes (transform track for active object)");
    const ObjectId target = sel.active_object;
    if (target == kInvalidObjectId) {
        ImGui::TextDisabled("No active object.");
        ImGui::End();
        return;
    }

    animation::TransformTrack* tr = anim.TryGetTrack(target);
    int key_count = tr ? static_cast<int>(tr->keys.size()) : 0;

    if (app.timeline_selected_key_object != target) {
        ClearTimelineKeyListSelection(app);
    } else if (tr && !FindKeyframeAtExactTime(*tr, app.timeline_selected_key_time, nullptr)) {
        ClearTimelineKeyListSelection(app);
    }

    if (ImGui::Button("Insert key at current time")) {
        scene::ObjectData* o = scene.TryGet(target);
        if (o) {
            animation::TransformTrack* tr_scan = anim.TryGetTrack(target);
            const float insert_t = AllocateInsertTimeForNewKey(anim, tr_scan, anim.current_time_seconds);
            if (insert_t < 0.f) {
                ImGui::TextColored(ImVec4(1.f, 0.4f, 0.35f, 1.f), "No free time before end of clip — extend duration or delete keys.");
            } else {
                animation::TransformKeyframe key{};
                key.time_seconds = insert_t;
                key.value = o->transform;

                const bool replaced = false;
                const animation::TransformKeyframe previous{};

                hist.Execute(commands::CommandHistory::MakeUpsertTransformKeyframe(target, key, replaced, previous), ctx);
                anim.current_time_seconds = insert_t;
                animation::EvaluateTransformTracksAtTime(anim, scene, anim.current_time_seconds);
                tr = anim.TryGetTrack(target);
                key_count = tr ? static_cast<int>(tr->keys.size()) : 0;
            }
        }
    }

    ImGui::SameLine();
    const bool can_delete_selected = TimelineKeyListSelectionValid(app, target, tr);
    bool can_delete_at_playhead = false;
    if (tr) {
        for (const auto& k : tr->keys) {
            if (k.time_seconds == anim.current_time_seconds) {
                can_delete_at_playhead = true;
                break;
            }
        }
    }
    const bool can_delete_any = can_delete_selected || can_delete_at_playhead;
    if (!can_delete_any) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Delete key")) {
        animation::TransformKeyframe removed{};
        bool found = false;
        if (tr && TimelineKeyListSelectionValid(app, target, tr)) {
            found = FindKeyframeAtExactTime(*tr, app.timeline_selected_key_time, &removed);
        } else if (tr) {
            for (const auto& k : tr->keys) {
                if (k.time_seconds == anim.current_time_seconds) {
                    removed = k;
                    found = true;
                    break;
                }
            }
        }
        if (found) {
            hist.Execute(commands::CommandHistory::MakeDeleteKeyframe(target, removed, 0), ctx);
            animation::EvaluateTransformTracksAtTime(anim, scene, anim.current_time_seconds);
            if (app.timeline_selected_key_object == target && app.timeline_selected_key_time == removed.time_seconds) {
                ClearTimelineKeyListSelection(app);
            }
            tr = anim.TryGetTrack(target);
            key_count = tr ? static_cast<int>(tr->keys.size()) : 0;
        }
    }
    if (!can_delete_any) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(list selection, else playhead)");

    tr = anim.TryGetTrack(target);
    key_count = tr ? static_cast<int>(tr->keys.size()) : 0;
    ImGui::Text("Keys: %d", key_count);
    ImGui::TextDisabled("If the playhead time is already keyed, insert uses the next free time (playhead follows).");

    ImGui::Separator();
    ImGui::TextUnformatted("Key list (click to select)");
    if (!tr || tr->keys.empty()) {
        ImGui::TextDisabled("(none)");
    } else {
        ImGui::BeginChild("keylist", ImVec2(0, 120), ImGuiChildFlags_Borders);
        const int n_rows = static_cast<int>(tr->keys.size());
        for (int i = 0; i < n_rows; ++i) {
            ImGui::PushID(i);
            const animation::TransformKeyframe& k = tr->keys[static_cast<std::size_t>(i)];
            const bool row_selected =
                (app.timeline_selected_key_object == target) && (app.timeline_selected_key_time == k.time_seconds);
            char label[160]{};
            std::snprintf(label, sizeof(label), "t=%.3f  pos=(%.2f, %.2f, %.2f)", k.time_seconds, k.value.translation.x, k.value.translation.y,
                          k.value.translation.z);
            if (ImGui::Selectable(label, row_selected)) {
                app.timeline_selected_key_object = target;
                app.timeline_selected_key_time = k.time_seconds;
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
    }

    (void)io;
    ImGui::End();
}

} // namespace aetdp1::timeline
