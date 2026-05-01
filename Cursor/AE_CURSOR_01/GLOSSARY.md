# AETDP1 — Glossary (local meanings)

## Scene data

The objects in `scene::SceneData` plus their fields (`Transform`, colors, names, ids) and the prototype’s **8-vertex local box** used for rendering and picking.

## Editor state

`editor::EditorState`: lightweight UI-ish mode/toggles (e.g., modeling vs animation tool mode). **Not** the same thing as “the whole app session”.

## Selection state

`selection::SelectionState`: which objects are selected, plus an **active object** used by inspector/timeline/gizmo as the primary target.

## Gizmo state

`gizmo::GizmoState`: hover/active axis + drag flag + sizing constants for the translate gizmo visualization/interaction.

## Command

A `commands::ICommand` implementing `Apply`/`Revert` against a `commands::CommandContext` (scene + selection + animation document). This is the **semantic edit boundary** for undo/redo in the prototype.

## Track

`animation::TransformTrack`: a list of transform keys for a specific `ObjectId`.

## Evaluator

`animation::EvaluateTransformTracksAtTime`: reads tracks/keys and computes a posed transform at a time. In this prototype it **writes results into `SceneData`** for visibility (prototype glue).

## Prototype glue

Code that exists only to keep the sample small and running end-to-end, but would likely change shape in Another Engine (e.g., evaluator mutating scene transforms directly, `AppState` aggregation, per-frame CPU mesh rebuild).

## Modeling tool path

UI + commands focused on objects in the scene: outliner, inspector, viewport picking, translate gizmo.

## Animation tool path

UI + data + evaluator focused on time-varying transforms: timeline transport, scrub, key insertion/deletion, interpolation.
