# AETDP1 — Reading Order

Read in this order for **maximum architectural signal per minute**. Paths are relative to `AETDP1/`.

## Shared preamble (5–10 minutes)

1. `src/core/types.h` — local identity + transform representation.
2. `src/scene/scene_data.h` — what “an object” is in this prototype (including the tiny mesh).
3. `src/selection/selection_state.h` — selection vs active object.
4. `src/editor/editor_state.h` — tool mode + a couple UI-ish toggles.
5. `src/commands/command_undo.h` + `src/modeling/modeling_commands.h` — command surface area (names only first).

## Modeling path (modeling tool first)

6. `src/modeling/modeling_commands.cpp` — default object creation helper (naming + unit box).
7. `src/commands/command_undo.cpp` — concrete commands + undo/redo skeleton (read class bodies slowly; it’s the spine).
8. `src/viewport/viewport_interaction.h` — camera + picking outputs.
9. `src/viewport/viewport_interaction.cpp` — AABB picking details.
10. `src/gizmo/gizmo_state.h` — gizmo UI-ish state (hover/active/drag).
11. `src/gizmo/translate_gizmo.h/.cpp` — screen-space hit test + drag mapping.
12. `src/imgui/imgui_layer.cpp` — how modeling UI routes into commands (outliner/inspector/viewport sections).
13. `src/app/app_state.h` — what the “session” aggregates (prototype glue, but readable).
14. `src/renderer/d3d12_renderer.h` — what rendering owns at the boundary level.
15. `src/renderer/d3d12_renderer.cpp` — only if you care about D3D12 mechanics (optional for architecture).

## Animation path (animation tool second)

16. `src/animation/animation_data.h/.cpp` — tracks/keys + upsert/delete rules.
17. `src/animation/animation_eval.h/.cpp` — interpolation + evaluation boundary (note the “writes scene” prototype glue).
18. `src/timeline/timeline_ui.h/.cpp` — timeline UI + transport + how it calls evaluation/commands.
19. `src/app/bootstrap_main.cpp` — playback tick (advances time while playing) and evaluation cadence.

## Persistence (cross-cutting, read after you understand edits)

20. `src/serialization/serialization.h/.cpp` — file format + load/save responsibilities.

## Bootstrap glue (read last)

21. `src/app/bootstrap_main.cpp` (Win32 + ImGui backend init section) — startup/shutdown ordering.
