# AETDP1 — Interaction Traces

This document traces **exact flows** through the prototype: files and primary functions, plus what data is read/written and why the responsibility belongs there.

Legend:
- **Read**: data consulted but not necessarily mutated.
- **Write**: data mutated (including via commands).

---

## 1) Selecting an object in the viewport

1. `src/imgui/imgui_layer.cpp` — `DrawViewport()`
   - **Read**: `ImGuiIO::MousePos`, viewport image rect (`viewport_ui.rect_min`), `app.viewport_ui.w/h`, `app.viewport.camera`, `app.scene`, `app.selection`, `app.editor.tool_mode`.
2. `src/viewport/viewport_interaction.cpp` — `OrbitCamera::ScreenPointToWorldRay(...)`
   - **Read**: camera parameters + viewport pixel size.
   - **Write**: none (computes `ray_origin`, `ray_dir`).
3. `src/viewport/viewport_interaction.cpp` — `ViewportInteraction::PickObject(...)`
   - **Read**: all `scene.objects`, each object’s `local_box` + `transform` to build world AABB.
   - **Write**: none (returns `PickResult`).
4. `src/imgui/imgui_layer.cpp` — constructs `selection::SelectionState before/after` and executes:
5. `src/commands/command_undo.cpp` — `CommandHistory::Execute(...)` with `CmdSelectObject` (via `MakeReplaceSelection`)
   - **Read**: `CommandContext` pointers.
   - **Write**: `selection::SelectionState` (selection + active object).

**Why here**: viewport computes geometry; selection mutation belongs in a command when you want a stable undo boundary (this prototype routes viewport clicks through commands).

---

## 2) Dragging the translate gizmo

1. `src/imgui/imgui_layer.cpp` — `DrawViewport()` on `ImGuiMouseButton_Left` click when modeling + active object:
   - **Read**: ray, camera, gizmo constants, active object translation.
2. `src/gizmo/translate_gizmo.cpp` — `TranslateGizmo::HitTestScreenSpace(...)`
   - **Read**: camera matrices, axis length, mouse position.
   - **Write**: none (returns axis).
3. `src/imgui/imgui_layer.cpp` — sets `app.gizmo_drag` (`transform_at_press`, `axis`, `gizmo_origin_press`) and `gizmo` drag flags.
4. While dragging: `src/gizmo/translate_gizmo.cpp` — `TranslateGizmo::TranslationDeltaFromMouseDelta(...)`
   - **Read**: camera basis derived from orbit parameters, mouse delta.
   - **Write**: none (returns delta vector).
5. `src/imgui/imgui_layer.cpp` — applies delta to `scene::ObjectData::transform.translation` for preview.
6. On mouse release: `src/commands/command_undo.cpp` — `CommandHistory::Execute(MakeSetTransform(...))`
   - **Write**: via `CmdSetTransform` applies final transform (undo restores press-time transform).

**Why here**: gizmo math is pure interaction; scene mutation during drag is preview glue; undo boundary is the command at release.

---

## 3) Creating an object

1. `src/imgui/imgui_layer.cpp` — `DrawOutliner()` button “Create box”
2. `src/modeling/modeling_commands.cpp` — `MakeDefaultNamedObject(...)`
   - **Read**: existing object names (for uniquification), allocates id via `scene::SceneData::AllocateId()`.
   - **Write**: none until command applies.
3. `src/commands/command_undo.cpp` — `CmdCreateObject::Apply`
   - **Write**: pushes into `scene.objects`.

**Why here**: modeling helper defines defaults; command owns insertion + undo removal.

---

## 4) Duplicating an object

1. `src/imgui/imgui_layer.cpp` — `DrawOutliner()` “Duplicate”
2. `src/commands/command_undo.cpp` — `CmdDuplicateObject::Apply` (first call builds snapshot + new id)
   - **Read**: source object from `scene`.
   - **Write**: appends duplicated `scene::ObjectData`.

**Why here**: duplication is a semantic modeling operation; undo removes the created id.

---

## 5) Deleting an object

1. `src/imgui/imgui_layer.cpp` — `DrawOutliner()` “Delete”
2. `src/commands/command_undo.cpp` — `CmdDeleteObject::Apply`
   - **Read**: snapshot + selection snapshot captured at command construction time (UI thread).
   - **Write**: removes object from `scene`, repairs `selection` if needed (`CleanupSelectionAfterDelete`).

**Why here**: delete must be reversible and must not orphan selection silently.

---

## 6) Inserting a transform keyframe

1. `src/timeline/timeline_ui.cpp` — `DrawTimelinePanel()` “Insert key at current time”
   - **Read**: `anim.current_time_seconds`, active object transform from `scene`.
2. `src/commands/command_undo.cpp` — `CmdAddKeyframe::Apply` / `MakeUpsertTransformKeyframe(...)`
   - **Write**: `animation::AnimationDocument` via `UpsertKeyframe`.
3. `src/animation/animation_eval.cpp` — `EvaluateTransformTracksAtTime(...)`
   - **Read**: tracks/keys.
   - **Write**: `scene` transforms for posed preview (prototype glue).

**Why here**: timeline UI owns widgets; animation document owns keys; evaluator owns interpolation; scene pose updates are explicitly called after edits for immediate feedback.

---

## 7) Scrubbing the timeline

1. `src/timeline/timeline_ui.cpp` — `ImGui::SliderFloat` on `anim.current_time_seconds`
   - **Write**: time directly (no per-tick command flood).
2. `src/animation/animation_eval.cpp` — `EvaluateTransformTracksAtTime(...)`
   - **Write**: scene transforms.

**Why here**: scrubbing is a high-frequency UI operation; the prototype prioritizes legibility over “everything is a command”.

---

## 8) Saving and loading the scene

### Save

1. `src/imgui/imgui_layer.cpp` — `DrawMainMenu()` File → Save (`GetSaveFileNameW`, `std::ofstream`)
2. `src/serialization/serialization.cpp` — `SaveTextDocument(...)`
   - **Read**: `scene`, `anim`.
   - **Write**: string/file bytes only.

### Load

1. `src/imgui/imgui_layer.cpp` — File → Load (`GetOpenFileNameW`, read file to string)
2. `src/serialization/serialization.cpp` — `LoadTextDocument(...)`
   - **Write**: replaces `scene` objects + `anim` tracks/time flags (parser-driven).
3. `src/imgui/imgui_layer.cpp` — clears selection + evaluates pose at current time.

**Why here**: serialization must not depend on viewport/gizmo/renderer; UI owns file picking; load resets session-ish fields explicitly.
