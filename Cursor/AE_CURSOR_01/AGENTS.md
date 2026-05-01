# AETDP1 — Agent / Contributor Guidance (`AGENTS.md`)

## Project purpose

**AETDP1** (**Another Engine — Top-Down Reference Prototype 1**) is a **support artifact for studying editor architecture**. It is **not** the official **Another Engine** repository, **not** production code, **not** a full engine, and **not** a generic framework.

The goal is **legibility + explicit boundaries** so patterns can be **manually translated** into the official Another Engine repo later.

## Hard constraints (do not drift)

- Keep it **self-contained** (Win32 + D3D12 + Dear ImGui + this repo’s modules).
- **Do not invent external integration APIs** or “hooks into Another Engine”.
- **Do not assume** integration points into the official repo.
- **Do not build** a reusable engine framework, ECS, reflection, plugins, asset pipeline, advanced shading, etc.
- Prefer **plain data + focused controllers** over clever abstractions.
- Prefer **explicit structure** over “generic everything”.
- Keep it **small enough to study later**.

## Product emphasis

- The **modeling tool** drives the prototype’s primary UX.
- The **animation tool** must fit **cleanly beside** modeling (timeline + evaluation), without taking over unrelated subsystems.

## Specialist-role ownership boundaries (from the prototype brief)

These boundaries are **design rules**, not runtime enforcement:

1. **Scene/Data**: owns objects/transforms/names/mesh corners; must not own timeline UI, gizmo logic, serialization, rendering policy.
2. **Modeling**: owns create/delete/duplicate + inspector-driven edits + tiny vertex demo; must not own animation timeline logic, serialization, renderer internals.
3. **Viewport/Picking/Gizmo**: owns camera navigation, picking, translate gizmo drag behavior; must not own animation model, save/load, command history policy.
4. **Animation/Timeline**: owns tracks/keys/time/playback/scrub UI + transform-track evaluation entrypoints; must not own scene persistence or modeling command logic.
5. **Commands/Undo**: owns semantic commands + routing + undo/redo skeleton; must not own raw rendering or low-level UI drawing.
6. **Serialization**: owns save/load + simple legible format; must not own viewport/gizmo/renderer internals.
7. **Review/Integration**: naming/boundary consistency; no late “engine features”.

## Decision rule (every change)

Ask:

1. Does this help understand the **modeling tool**?
2. Does this help understand the **animation tool**?
3. Is this a **real structural idea** or **prototype glue**?
4. Is it still **small enough to study later**?

If you cannot answer (1) or (2), the change is probably out of scope.

## Non-goals

- “Make it a real renderer”
- “Make it a real asset pipeline”
- “Make it general-purpose”

## Repo naming note

**Another Engine** is the official name. **AETDP1** is only this prototype’s label.
