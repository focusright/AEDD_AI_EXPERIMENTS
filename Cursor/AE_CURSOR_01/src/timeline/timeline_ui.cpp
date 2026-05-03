#include "timeline/timeline_ui.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>

#include "animation/animation_data.h"
#include "core/types.h"
#include "animation/animation_eval.h"
#include "commands/command_undo.h"
#include "modeling/modeling_commands.h"
#include "scene/scene_data.h"
#include "selection/selection_state.h"

namespace aetdp1::timeline {

void DrawTimelinePanel(animation::AnimationDocument& anim, const selection::SelectionState& sel, scene::SceneData& scene, commands::CommandHistory& hist,
                       commands::CommandContext& ctx, const ImGuiIO& io) {
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
    const int key_count = tr ? static_cast<int>(tr->keys.size()) : 0;
    ImGui::Text("Keys: %d", key_count);

    if (ImGui::Button("Insert key at current time")) {
        scene::ObjectData* o = scene.TryGet(target);
        if (o) {
            animation::TransformKeyframe key{};
            key.time_seconds = anim.current_time_seconds;
            key.value = o->transform;

            bool replaced = false;
            animation::TransformKeyframe previous{};
            if (tr) {
                for (const auto& k : tr->keys) {
                    if (k.time_seconds == key.time_seconds) {
                        replaced = true;
                        previous = k;
                        break;
                    }
                }
            }

            hist.Execute(commands::CommandHistory::MakeUpsertTransformKeyframe(target, key, replaced, previous), ctx);
            animation::EvaluateTransformTracksAtTime(anim, scene, anim.current_time_seconds);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Delete key at current time")) {
        if (tr) {
            animation::TransformKeyframe removed{};
            bool found = false;
            for (const auto& k : tr->keys) {
                if (k.time_seconds == anim.current_time_seconds) {
                    removed = k;
                    found = true;
                    break;
                }
            }
            if (found) {
                hist.Execute(commands::CommandHistory::MakeDeleteKeyframe(target, removed, 0), ctx);
                animation::EvaluateTransformTracksAtTime(anim, scene, anim.current_time_seconds);
            }
        }
    }

    ImGui::Separator();
    ImGui::Text("Key list (read-only)");
    if (!tr || tr->keys.empty()) {
        ImGui::TextDisabled("(none)");
    } else {
        ImGui::BeginChild("keylist", ImVec2(0, 120), true);
        for (const auto& k : tr->keys) {
            ImGui::BulletText("t=%.3f pos=(%.2f, %.2f, %.2f)", k.time_seconds, k.value.translation.x, k.value.translation.y, k.value.translation.z);
        }
        ImGui::EndChild();
    }

    (void)io;
    ImGui::End();
}

} // namespace aetdp1::timeline
