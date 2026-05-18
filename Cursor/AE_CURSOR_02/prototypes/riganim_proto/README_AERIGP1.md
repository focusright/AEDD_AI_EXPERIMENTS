# AERIGP1 — Another Engine Rig/Animation Prototype 1

A small, readable **Win32 + Direct3D 12 + Dear ImGui** reference prototype for studying character rigging and animation editor architecture. It is **not** the official Another Engine product; it lives in `prototypes/riganim_proto/` so the main AE tree stays clean.

## What this demonstrates

- Central **rig document** (mesh, skeleton, skin weights, pose, animation clips, editor mode)
- Procedural low-poly humanoid + default 19-joint skeleton
- **CPU skinning** with up to 4 influences per vertex
- **Rig / Skin / Animate** ImGui tool modes
- 24 FPS animation with per-joint tracks (lerp position/scale, slerp rotation)
- Simple **2-bone IK** on the left arm chain
- Text **save/load** (`.aerig`, `.aeanim`)
- Validation panel + debug console output

## Build and run

**Requirements:** Windows 10+, Visual Studio 2022 (v143), Windows SDK, D3D12.

```bat
cd prototypes\riganim_proto
build_vs2022.bat
bin\x64\Debug\AERIGP1.exe
```

First build runs `scripts\FetchImGui.ps1` (copies ImGui from `AE_CURSOR_01` or clones from GitHub).

Bundled samples live in `data/`:

- `data/sample_character.aerig`
- `data/sample_wave.aeanim`

Regenerate them after building:

```bat
bin\x64\Debug\AERIGP1.exe --export-samples
```

## Controls

| Input | Action |
|--------|--------|
| **Rig / Skin / Animate** radio | Switch editor mode |
| Joint hierarchy list | Select joint |
| Position / Rotation (deg) / Scale | Edit selected joint local pose (Rig mode) |
| Viewport **right-drag** | Orbit camera |
| Viewport **mouse wheel** | Zoom |
| Play / Frame slider | Animate mode playback and scrubbing |
| IK target + checkbox | 2-bone IK on left arm (Rig mode) |
| Save/Load buttons | File dialogs for `.aerig` / `.aeanim` |

## Code file overview

| File | Role |
|------|------|
| `src/rig_types.h` | `RigDocument`, `Joint`, `Skeleton`, `Mesh`, `AnimClip`, … |
| `src/transform.*` | `Transform`, matrix helpers, Euler ↔ quaternion |
| `src/skeleton.*` | Hierarchy, world transforms, inverse bind |
| `src/procedural_character.*` | Default mesh + skeleton + sample wave clip |
| `src/skinning.*` | Auto-weight, normalize, CPU deform |
| `src/animation.*` | Keyframes, evaluation, playback tick |
| `src/ik_two_bone.*` | Readable 2-bone IK reference |
| `src/rig_serializer.*` | `.aerig` / `.aeanim` IO |
| `src/rig_validator.*` | Hierarchy + skin + track checks |
| `src/rig_renderer.*` | D3D12 viewport (mesh + skeleton + grid) |
| `src/rig_ui.*` | ImGui panels and app wiring |
| `src/main.cpp` | Win32 loop, ImGui DX12 init |

## Data model overview

- **`RigDocument`** owns mesh, skeleton, bind pose copy, clips, playback state, IK settings, skinned vertex cache, validation messages.
- **`Joint`**: name, parent index, local bind/pose, world bind/pose, inverse bind matrix, display color.
- **`SkinWeight`**: 4 joint indices + weights (normalized for rendering).
- **`AnimClip`**: name, FPS (24), frame count, `AnimTrack` per joint, `AnimKey` per frame with local `Transform`.

Editor modes:

1. **Rig** — hierarchy, joint TRS, reparent, bind recompute, IK demo.
2. **Skin** — auto-weight, normalize, weight debug coloring for selected joint.
3. **Animate** — timeline, keys, play/scrub, save/load clip.

## Rig file format (`AERIG_V1`)

Plain text, line-oriented:

```
AERIG_V1
joint_count N
joint <name> <parent> px py pz qx qy qz qw sx sy sz
...
vertex_count N
vertex px py pz nx ny nz cr cg cb ca  (4x joint_index weight pairs)
...
index_count N
index i
...
end
```

## Animation file format (`AEANIM_V1`)

```
AEANIM_V1
name <clip_name>
fps 24
frame_count 48
track_count N
track <joint_index> <key_count>
key <frame> px py pz qx qy qz qw sx sy sz
...
end
```

## CPU skinning

For each mesh vertex in bind pose:

1. Build per-joint skin matrix: `skinMat[j] = inverse_bind[j] * world_pose[j]`.
2. For each influence `i` with weight `w` and joint `j`:
   - `position += w * (skinMat[j] * bindPosition)`
   - `normal += w * (skinMat[j] * bindNormal)` (then normalize)

Implemented in `skinning.cpp` (`SkinningCpuDeform`) — intentionally straightforward loops, no GPU path.

## Animation evaluation

1. For each `AnimTrack`, find bracketing keys at the current frame.
2. `u = (frame - f0) / (f1 - f0)`; lerp translation/scale, slerp rotation.
3. Write `local_pose` on each joint; `SkeletonUpdateWorldTransforms`.
4. Run CPU skinning.

Playback advances `current_frame` at 24 FPS when **Play** is enabled (`AnimationTickPlayback`).

## Mapping to future AE editor modes

| AERIGP1 | Future AE |
|---------|-----------|
| Rig mode + joint TRS | **Rigging** mode: skeleton authoring |
| Skin weights + debug colors | **Rigging**: weight painting tools |
| Animate + timeline | **Animation** mode + **Sequencer** clips |
| Procedural mesh | **Modeling** import/generation (replace with real assets later) |
| `.aerig` / `.aeanim` | Starting point for AE asset schemas |

See also `INTEGRATION_NOTES.md`.
