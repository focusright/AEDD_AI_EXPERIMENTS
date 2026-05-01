# AETDP1 — File Ownership

For every source/header in this prototype: **owns / may call / must not own / public entry points**.

## `src/app/bootstrap_main.cpp`

- **Owns**: Win32 window class + message pump, Dear ImGui backend initialization, per-frame ordering (playback tick → UI → viewport resize → render).
- **May call**: renderer (`Create`, `Resize`, `ResizeViewportTexture`, `RenderFrame`, `WaitForGpuIdle`, `Destroy`), ImGui backends, `imgui_layer::BuildFrame`, `animation_eval`.
- **Must not own**: scene editing rules (delegates to UI/commands), serialization UI (in imgui layer), gizmo math.
- **Public entry points**: `wWinMain` (application entry).

## `src/app/app_state.h`

- **Owns**: POD/session aggregation for the prototype (`AppState`), UI drag bookkeeping, inspector baselines.
- **May call**: (header only) none.
- **Must not own**: behavioral logic (should remain mostly data).
- **Public entry points**: types/fields consumed by `imgui_layer` and `bootstrap_main`.

## `src/core/types.h`

- **Owns**: `ObjectId`, `Transform`, `TransformToMatrix` helper.
- **May call**: DirectXMath.
- **Must not own**: scene containers, UI, commands.
- **Public entry points**: `TransformToMatrix`, aliases.

## `src/scene/scene_data.h/.cpp`

- **Owns**: `SceneData` container rules (`TryGet`, `AllocateId`, `RemoveById`), default box corner initialization.
- **May call**: `<algorithm>` for erase/remove patterns.
- **Must not own**: selection policy, undo, animation evaluation, rendering.
- **Public entry points**: `SceneData::{TryGet,IndexOf,AllocateId,RemoveById,EnsureDefaultBoxCorners}`.

## `src/selection/selection_state.h/.cpp`

- **Owns**: selection set + `active_object` mutations helpers.
- **May call**: `<algorithm>`.
- **Must not own**: scene objects’ fields, commands, picking.
- **Public entry points**: `Clear`, `SetSingleSelection`, `SetActive`, `Remove`, `IsSelected`.

## `src/editor/editor_state.h/.cpp`

- **Owns**: `EditorToolMode` + a couple toggles.
- **May call**: none.
- **Must not own**: scene truth, animation truth.
- **Public entry points**: enum + struct fields.

## `src/viewport/viewport_interaction.h/.cpp`

- **Owns**: orbit camera matrices, screen→world ray, object picking against AABBs.
- **May call**: `TransformToMatrix`, DirectXMath.
- **Must not own**: selection mutation policy, commands, animation.
- **Public entry points**: `OrbitCamera::{View,Projection,ScreenPointToWorldRay}`, `ViewportInteraction::{OrbitFromMouseDelta,PickObject}`.

## `src/gizmo/gizmo_state.h/.cpp`

- **Owns**: gizmo mode flags (mostly empty cpp by design).
- **May call**: none.
- **Must not own**: scene transforms, undo, picking, rendering meshes.
- **Public entry points**: `GizmoState` fields, `GizmoAxis`.

## `src/gizmo/translate_gizmo.h/.cpp`

- **Owns**: screen-space axis hit testing + mouse delta mapping along axis.
- **May call**: `viewport::OrbitCamera`, DirectXMath.
- **Must not own**: commands, scene containers, serialization.
- **Public entry points**: `TranslateGizmo::{HitTestScreenSpace,TranslationDeltaFromMouseDelta}`.

## `src/modeling/modeling_commands.h/.cpp`

- **Owns**: `CommandContext` declaration, `MakeDefaultNamedObject` helper, `ICommand` forward usage.
- **May call**: `scene::SceneData` helpers.
- **Must not own**: undo stack mechanics (lives in `command_undo.cpp`), animation UI.
- **Public entry points**: `MakeDefaultNamedObject`, `CommandContext`, `ICommand`.

## `src/commands/command_undo.h/.cpp`

- **Owns**: `CommandHistory`, concrete `ICommand` types, command factories (`Make*`).
- **May call**: scene/selection/animation mutations necessary to implement Apply/Revert.
- **Must not own**: Win32, D3D12, Dear ImGui drawing, file dialogs.
- **Public entry points**: `CommandHistory::{Execute,Undo,Redo,Clear}`, `Make*` factories.

## `src/animation/animation_data.h/.cpp`

- **Owns**: animation document structure, key ordering/upsert/delete rules for tracks.
- **May call**: `<algorithm>`.
- **Must not own**: evaluation math (separate file), UI widgets, serialization text grammar.
- **Public entry points**: `AnimationDocument::{FindTrackIndex,TryGetTrack,UpsertKeyframe,TryDeleteKeyframeAtTime}`.

## `src/animation/animation_eval.h/.cpp`

- **Owns**: interpolation + evaluation at a time.
- **May call**: `scene::SceneData` write path for posed transforms (**prototype glue**).
- **Must not own**: UI, undo, file IO.
- **Public entry points**: `EvaluateTransformTracksAtTime`.

## `src/timeline/timeline_ui.h/.cpp`

- **Owns**: Dear ImGui timeline/transport widgets and their immediate behavior wiring.
- **May call**: `animation_eval`, `command_undo`, `scene` (read transforms for key insertion), `ImGui`.
- **Must not own**: D3D12, Win32 message loop, serialization file picker implementation.
- **Public entry points**: `DrawTimelinePanel`.

## `src/serialization/serialization.h/.cpp`

- **Owns**: `AETDP1_TEXT_V1` text format parsing/serialization.
- **May call**: `<sstream>`, string utilities.
- **Must not own**: viewport/gizmo/renderer, command routing.
- **Public entry points**: `SaveTextDocument`, `LoadTextDocument`.

## `src/renderer/d3d12_renderer.h/.cpp`

- **Owns**: D3D12 device/swapchain, descriptor heaps, viewport offscreen target + depth, mesh upload buffer, draw passes, ImGui swapchain pass invocation.
- **May call**: D3DCompiler, WRL, ImGui DX12 backend headers, scene/selection for draw iteration.
- **Must not own**: editor commands, animation UI, file picking.
- **Public entry points**: `D3d12Renderer::{Create,Destroy,BeginResize,Resize,ResizeViewportTexture,RenderFrame,ViewportSrvGpuHandle,device,command_queue,command_list,srv_heap,AllocSrvForImGui,FreeSrvForImGui,WaitForGpuIdle}`.

## `src/renderer/shaders.hlsl`

- **Owns**: reference HLSL text (not compiled by MSBuild in this project; shader is embedded as a string in `d3d12_renderer.cpp`).
- **May call**: n/a (not built as a separate compile unit here).
- **Must not own**: runtime logic.
- **Public entry points**: informational only.

## `src/imgui/imgui_layer.h/.cpp`

- **Owns**: windowing/layout of editor panels + routing user actions to commands / evaluation / file IO.
- **May call**: Win32 file dialogs, `scene/selection/commands/animation/serialization/viewport/gizmo`.
- **Must not own**: D3D12 device creation details (bootstrap/renderer), low-level mesh algorithms (renderer).
- **Public entry points**: `BuildFrame`.

## Third-party Dear ImGui (`third_party/imgui/**`)

- **Owns**: Dear ImGui library implementation (upstream).
- **Must not be treated as** Another Engine code; keep updates isolated.
