# AERIGP1 — Integration notes for Another Engine

This prototype is a **study reference**, not a drop-in module. Below are the pieces most worth manually porting into AE’s real rigging/animation editor.

## High value to port

### Skeleton data model (`rig_types.h`, `skeleton.cpp`)

- `Joint` with parent index, local bind/pose, world matrices, inverse bind.
- `SkeletonUpdateWorldTransforms` and `SkeletonRecomputeInverseBind` are the core pose pipeline.
- Cycle detection on reparent (`SkeletonHasCycle`) maps directly to rig validation in AE.

### Pose evaluation (`animation.cpp`)

- Per-joint tracks with frame keys and `LerpTransform` (position/scale lerp, rotation slerp).
- Fixed 24 FPS stepping is a simple contract for a first AE animation timeline.
- Keep **bind pose** and **evaluated pose** separate in production (this prototype sometimes conflates them for brevity).

### CPU skinning reference (`skinning.cpp`)

- Clear `inverse_bind * world_pose` matrix palette and weighted accumulation.
- Use as golden reference for GPU skinning shader tests later.
- Auto-weight-by-distance is a reasonable default bind tool before manual paint.

### Animation clip / keyframe model (`AnimClip`, `AnimTrack`, `AnimKey`)

- Text-friendly schema in `.aeanim` — evolve into AE JSON/binary assets.
- Keyframe = local transform at integer frame — matches DCC expectations.

### Timeline UI ideas (`rig_ui.cpp` Animate section)

- Frame slider, play/pause, set/delete key on selected joint.
- Extend with curve editor, multi-select tracks, and undo in AE.

### Rig validation (`rig_validator.cpp`)

- Parent index, cycles, root, weight sum, orphan vertices, invalid track joints.
- Run on save and show in a docked panel in AE.

### Save/load schema (`rig_serializer.cpp`)

- `AERIG_V1` / `AEANIM_V1` headers for forward-compatible parsers.
- Split rig mesh/skeleton/weights from animation clips (already separate files).

## Medium value

- **2-bone IK** (`ik_two_bone.cpp`): good teaching code; AE will want full IK chains, poles, and constraints.
- **Procedural character** (`procedural_character.cpp`): replace with modeled assets; keep as unit-test geometry.
- **D3D12 viewport** (`rig_renderer.cpp`): follow AE’s renderer; reuse only the debug draw ideas (bones, joints, weight colors).

## Low priority / do not port verbatim

- Single `RigDocument` god-object — AE should split assets, scene instances, and editor session state.
- ImGui layout code — rewrite against AE’s UI framework/panels.
- WARP/hardware fallback renderer — use AE’s device init.

## Suggested AE porting order

1. Skeleton + bind pose + inverse bind utilities (unit tests only).
2. CPU skinning path + debug draw of bones/weights.
3. Animation clip IO + evaluator at 24 FPS.
4. Rigging editor mode (hierarchy + TRS + validation).
5. Animation mode + sequencer integration.
6. GPU skinning and IK/FK tooling.

## Related reference in this repo

`Cursor/AE_CURSOR_01` (AETDP1) covers general editor architecture (commands, timeline patterns, D3D12+ImGui bootstrap). AERIGP1 focuses specifically on **rigging and skinning**.
