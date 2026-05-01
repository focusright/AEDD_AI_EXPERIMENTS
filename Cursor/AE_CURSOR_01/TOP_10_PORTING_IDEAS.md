# AETDP1 — Top 10 Porting Ideas (manual translation into Another Engine)

These are ranked for **conceptual value** vs **disposable prototype code**.

## 1) Semantic command objects for modeling edits (high concept / medium disposable)

**Why**: stable undo boundaries and clearer team review than ad-hoc mutation.
**Disposable**: exact `ICommand` shape; Another Engine may use different allocation, merging, coalescing.

## 2) Separate selection vs active object (high concept / low disposable)

**Why**: future editor modes (property panels, animation targets, context menus) want a crisp “primary subject”.
**Disposable**: `SelectionState` struct layout.

## 3) Viewport picking as a pure geometric service (high concept / medium disposable)

**Why**: keeps input interpretation out of command logic and out of UI widgets.
**Disposable**: AABB-only picking; ray helper signatures.

## 4) Timeline UI vs track data vs evaluator files (high concept / low disposable)

**Why**: this is one of the most “translation-friendly” splits in the prototype.
**Disposable**: function names; exact key container (`std::vector`) choices.

## 5) Explicit animation document (`AnimationDocument`) (high concept / medium disposable)

**Why**: gives you a place to grow curves, layers, clips without entangling scene storage prematurely.
**Disposable**: storing playback flags inside the same struct.

## 6) Serialization boundary (`serialization/`) (high concept / medium disposable)

**Why**: prevents “save format knowledge” from spreading across UI and scene code.
**Disposable**: the exact `AETDP1_TEXT_V1` grammar (replace with real format later).

## 7) Gizmo interaction split (`gizmo/` vs `commands/`) (high concept / high disposable)

**Why**: interaction math and undo policy are different axes of change.
**Disposable**: screen-space hit testing specifics; immediate-mode coupling.

## 8) “Commit” style inspector for transform undo (medium concept / high disposable)

**Why**: teaches undo flooding hazards and a pragmatic mitigation in a tiny UI.
**Disposable**: button-based workflow; Another Engine may use property transactions.

## 9) Playback tick location in bootstrap loop (medium concept / high disposable)

**Why**: clarifies ordering: advance time → evaluate pose → render.
**Disposable**: exact placement; engines usually centralize time in a clock service.

## 10) Offscreen viewport render target for UI embedding (low concept for AE / high disposable)

**Why**: useful for studying “tool UI + 3D view” composition on Win32.
**Disposable**: entire D3D12 approach if Another Engine uses a different RHI/UI stack.
