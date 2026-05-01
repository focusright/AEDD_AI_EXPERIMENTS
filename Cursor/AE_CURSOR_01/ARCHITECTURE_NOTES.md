# AETDP1 — Architecture Notes

AETDP1 is a **small, self-contained top-down reference prototype** for studying editor-shaped boundaries. The official product is **Another Engine**; this repository is **not** that product and intentionally avoids “engine framework” sprawl.

## What each subsystem owns (real boundaries)

- **`app/` bootstrap**: Win32 lifetime, main loop ordering, Dear ImGui + DX12 backend wiring, and **prototype session wiring** (`AppState`, `CommandContext` pointers). Owns shutdown ordering (GPU idle before ImGui DX12 teardown).
- **`renderer/`**: D3D12 device/swapchain, an **offscreen viewport color + depth target**, a minimal triangle/line pipeline, and ImGui’s swapchain pass. Owns GPU resource lifetime for those targets — **not** scene semantics, commands, or UI layout policy.
- **`scene/`**: Authoring-time object records: id/name/color, `Transform`, and a tiny **local box mesh** (8 verts) used for picking + the vertex nudge demo. Owns object identity and transform fields as plain data — **not** UI, undo, animation policy, or rendering policy beyond “what mesh exists”.
- **`editor/`**: Lightweight editor-facing toggles (tool mode, selection highlight flag). Owns **not** scene truth, animation truth, or commands.
- **`selection/`**: Selected ids + `active_object`. Owns selection structure — **not** how selection is chosen (viewport does picking; commands apply changes).
- **`viewport/`**: Orbit camera + ray picking against world AABBs derived from mesh corners. Owns camera + picking geometry — **not** command history, serialization, or gizmo semantics.
- **`gizmo/`**: Translate gizmo hover/active state + screen-space hit test + a small “mouse delta → axis motion” mapping. Owns gizmo interaction geometry — **not** undo policy (commands are executed at drag end).
- **`modeling/`**: “What a default object looks like” helpers (`MakeDefaultNamedObject`) used by modeling flows. Owns naming/default mesh initialization helpers — **not** undo stack mechanics.
- **`commands/`**: `ICommand` + `CommandHistory` + concrete commands (create/delete/duplicate/select/transform/vertex/key/time). Owns undo/redo skeleton and semantic edit boundaries — **not** rendering, timeline UI, or file IO.
- **`animation/` data**: `AnimationDocument`, tracks, keys, playback flags, duration/time. Owns animation authoring data — **not** evaluation implementation details beyond storage.
- **`animation/` eval**: Interpolation + applying evaluated transforms into `SceneData` for preview. Owns math — **note: writing into `SceneData` here is prototype glue** (see below).
- **`timeline/`**: Dear ImGui widgets for transport + scrub + key ops (calls commands for key changes). Owns **UI** — **not** evaluation math (calls `animation_eval`).
- **`serialization/`**: Text snapshot format (`AETDP1_TEXT_V1`) for scene + animation. Owns file grammar — **not** viewport, gizmo, or GPU policy.

## What is “real structural idea” vs “prototype glue”

### Real structural ideas (worth carrying as concepts)

- **Explicit module ownership** (scene vs selection vs viewport vs gizmo vs commands vs animation vs serialization).
- **Semantic commands** as the stable edit boundary for modeling + keyframe edits (even if the command set is tiny).
- **Selection vs active object** split (set is small; active drives inspector/timeline/gizmo).
- **Timeline UI vs track data vs evaluator** separation in files and call flow.
- **Serialization as a separate boundary** from runtime interaction.

### Prototype glue (disposable or risky to copy verbatim)

- **Evaluator writes directly into `scene::SceneData` transforms** for preview/playback/scrub. This keeps the prototype small, but it blurs “authoring transform” vs “posed transform”. A production tool would usually separate **bind pose / authored state** from **evaluated pose** (or use overlays).
- **`AppState` as a mini session object** holding everything: fine for study, not a long-term architecture.
- **Inspector mostly edits scene live** with explicit “Commit transform (undoable)” to avoid flooding undo during drags: pragmatic, not a full property system.
- **D3D12 renderer uploads rebuilt mesh vertices each frame** from CPU: fine for a handful of boxes; not a renderer strategy.
- **Scrubbing time mutates `AnimationDocument::current_time_seconds` directly** (not a per-scrub command flood). Undo for time exists as a command type, but UI doesn’t push it every scrub tick.

## What seems worth carrying into Another Engine later (concept-first)

- **Command boundary discipline** around modeling edits (create/delete/duplicate/transform) and a parallel boundary for animation keys.
- **Selection + active object** as separate concerns from “what is selected in the viewport right now”.
- **Viewport picking as a pure geometric service** returning intents/ids, with selection changes routed through commands when you want undo.
- **Track/key container** shape (`TransformTrack` + sorted keys) as a readable starting point for richer animation data later.
- **Serialization module** that can evolve independently from UI layout.

## Modeling vs animation driving the prototype

Modeling is the default “feel” of the UI (outliner/inspector/viewport/gizmo). Animation sits beside it as a **second tool surface** (timeline) with explicit evaluation separation in code, even though both touch transforms in this small prototype.
