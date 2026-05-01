# AETDP1 — Function Index

Grouped by subsystem. “Study first” marks the highest-signal entry points for understanding the prototype.

## App / bootstrap

| Function | What it does | Study first? |
|---|---|---|
| `wWinMain` (`src/app/bootstrap_main.cpp`) | Creates window, initializes D3D12 + ImGui DX12, wires callbacks, runs loop (playback tick → UI → render), shuts down in a safe GPU order. | **Yes** |

## ImGui layer (UI routing)

| Function | What it does | Study first? |
|---|---|---|
| `aetdp1::imgui_layer::BuildFrame` | Top-level UI: main menu, outliner/inspector/viewport/timeline; routes modeling actions to commands; routes animation scrub to evaluator. | **Yes** |
| `DrawViewport` (anonymous, `src/imgui/imgui_layer.cpp`) | Viewport image + mouse routing: orbit, pick, gizmo drag preview, selection commands, gizmo commit command. | **Yes** |
| `DrawOutliner` | Create/duplicate/delete + object list selection via commands. | Yes |
| `DrawInspector` | Edits scene fields + explicit commit buttons for undoable transform/vertex nudge. | Yes |
| `DrawMainMenu` | File save/load + undo/redo menu items. | Medium |

## Scene

| Function | What it does | Study first? |
|---|---|---|
| `scene::SceneData::TryGet` | Stable lookup of an object by id. | Yes |
| `scene::SceneData::EnsureDefaultBoxCorners` | Ensures the 8 local verts exist (unit cube heuristic). | Medium |

## Selection

| Function | What it does | Study first? |
|---|---|---|
| `selection::SelectionState::SetSingleSelection` | Sets selection + active object together (prototype simplification). | Yes |

## Viewport

| Function | What it does | Study first? |
|---|---|---|
| `viewport::OrbitCamera::ScreenPointToWorldRay` | Builds a world-space pick ray from viewport-local pixels. | **Yes** |
| `viewport::ViewportInteraction::PickObject` | Ray vs world AABB tests for each object; returns best hit. | **Yes** |
| `viewport::ViewportInteraction::OrbitFromMouseDelta` | Updates orbit camera yaw/pitch with clamped pitch. | Medium |

## Gizmo

| Function | What it does | Study first? |
|---|---|---|
| `gizmo::TranslateGizmo::HitTestScreenSpace` | Picks closest axis segment in screen space. | Yes |
| `gizmo::TranslateGizmo::TranslationDeltaFromMouseDelta` | Maps mouse delta to motion along a world axis using camera basis. | Yes |

## Commands / undo

| Function | What it does | Study first? |
|---|---|---|
| `commands::CommandHistory::Execute` | Applies command then pushes undo stack entry; clears redo. | **Yes** |
| `commands::CommandHistory::Undo` / `Redo` | Pops stacks and calls `Revert`/`Apply`. | **Yes** |
| `commands::CommandHistory::MakeReplaceSelection` | Factory for selection snapshot commands. | Yes |
| `commands::CommandHistory::MakeSetTransform` | Factory for transform before/after commands. | Yes |
| `commands::CommandHistory::MakeUpsertTransformKeyframe` | Factory for keyframe upsert with replace metadata. | Medium |

## Animation data

| Function | What it does | Study first? |
|---|---|---|
| `animation::AnimationDocument::UpsertKeyframe` | Maintains sorted keys; replaces same-time keys. | **Yes** |
| `animation::AnimationDocument::TryDeleteKeyframeAtTime` | Deletes a key at an exact time if present. | Yes |

## Animation evaluation

| Function | What it does | Study first? |
|---|---|---|
| `animation::EvaluateTransformTracksAtTime` | Interpolates transform keys and writes posed transforms into `SceneData` (prototype glue). | **Yes** |

## Timeline UI

| Function | What it does | Study first? |
|---|---|---|
| `timeline::DrawTimelinePanel` | Transport + scrub + key list + insert/delete key using commands + evaluator refresh. | **Yes** |

## Serialization

| Function | What it does | Study first? |
|---|---|---|
| `serialization::SaveTextDocument` | Serializes scene + animation into `AETDP1_TEXT_V1` text. | Yes |
| `serialization::LoadTextDocument` | Parses text; fills `SceneData` + `AnimationDocument` or returns error string. | Yes |

## Renderer (optional for architecture readers)

| Function | What it does | Study first? |
|---|---|---|
| `renderer::D3d12Renderer::RenderFrame` | Records viewport 3D pass then ImGui draw to swapchain; presents. | Medium |
| `renderer::D3d12Renderer::ResizeViewportTexture` | Recreates offscreen color/depth targets to match the ImGui viewport size. | Medium |

## Modeling helpers

| Function | What it does | Study first? |
|---|---|---|
| `commands::MakeDefaultNamedObject` | Allocates id + default transform/color + unit box corners + uniquified name. | Yes |
