# AETDP1 — Extractable Patterns

## Useful modeling-tool patterns

- **Outliner drives selection via commands** (`MakeReplaceSelection`) so selection changes can participate in undo where desired.
- **Inspector “commit” button** for transform edits to avoid undo flooding during continuous drags.
- **Viewport picking as a pure function of scene + ray** returning a pick intent/id (`viewport::PickObject`).
- **Gizmo hover vs active axis** separated from scene data; drag previews mutate scene, command commits on mouse up.
- **Create/Duplicate/Delete** modeled as explicit commands with enough snapshot data to revert.

## Useful animation-tool patterns

- **Tracks keyed by `ObjectId`** (simple association model).
- **Sorted keys + bracket search** for evaluation.
- **Separate files**: `animation_data` vs `animation_eval` vs `timeline_ui`.
- **Transport state** (`playing`/`paused`) kept in animation document (small, explicit).

## Useful command boundaries

- **One semantic user action → one command** for: create/delete/duplicate, selection replacement, transform commit, key upsert/delete.
- **CommandContext** is intentionally small: only the world slices commands may mutate.

## Useful data boundaries

- **Scene vs selection vs editor toggles** are separate structs.
- **Serialization** reads/writes both scene + animation without knowing UI.

## Risky patterns (do not copy blindly into Another Engine)

- **Evaluator writes scene transforms directly** (poses and authoring collide).
- **Playback + inspector both mutate transforms** without a stronger pose model.
- **CPU-rebuilt mesh every frame** in the renderer.
- **Ray/AABB picking only** (no mesh-accurate picking; no materials; no layers).
