# AETDP1 — Claude Code / Agent Memory (`CLAUDE.md`)

## What this repository is

This is **AETDP1**: a **small Win32 + Direct3D 12 + Dear ImGui** reference prototype for studying **editor architecture** (modeling-first, animation beside it). It is explicitly **not** the official **Another Engine** repository and must not be treated as production engine code.

## Primary goals

- Make **data boundaries** and **control boundaries** obvious: scene vs selection vs editor vs viewport vs gizmo vs commands vs animation vs serialization.
- Keep code **readable and reviewable** for later **manual translation** into Another Engine.
- Avoid “engine-building drift”: no ECS, no plugins, no asset pipeline, no advanced rendering.

## Modeling vs animation

- **Modeling** drives the default workflow (outliner/inspector/viewport/gizmo).
- **Animation** is required and must remain a **clean parallel**: timeline UI + track data + evaluator are separated in modules.

## Ownership rules (do not violate casually)

- **Scene** owns object records and transforms as plain data.
- **Selection** owns selected ids + active object.
- **Viewport** owns camera + picking geometry (rays/AABBs).
- **Gizmo** owns interaction mapping for the translate gizmo.
- **Commands** own undoable semantic edits and routing via `CommandContext`.
- **Animation data** owns tracks/keys; **animation eval** owns interpolation; **timeline UI** owns Dear ImGui widgets.
- **Serialization** owns file format read/write.
- **Renderer** owns GPU resources and drawing the placeholder scene into an offscreen viewport texture.

## Prototype glue (expect to replace in a real editor)

- Evaluator **writes poses into `SceneData`** for visibility.
- `AppState` aggregates subsystems for convenience.
- Inspector uses explicit “commit” buttons for some undoable edits.

## Engineering style preferences

- Prefer **boring explicit code** over templates/metaprogramming.
- Prefer **small files with clear names** over “ultimate abstractions”.
- When adding a feature, update the learning artifacts (`ARCHITECTURE_NOTES.md`, `FILE_OWNERSHIP.md`, `INTERACTION_TRACES.md`) if boundaries move.

## Build note

Fetch Dear ImGui into `third_party/imgui` using `scripts/FetchImGui.ps1` before opening the `.sln` in Visual Studio.
