# Heat Method DX12 + ImGui Demo Spec for Codex

Create a standalone Windows C++ DirectX 12 + Dear ImGui demo implementing the heat method from Crane, Weischedel, and Wardetzky’s “Geodesics in Heat.”

This is not an integration into an existing engine. The only engine-stack requirement is: native C++ + DirectX 12 + Dear ImGui.

The demo should render a triangulated sphere, compute geodesic distance from a selected source vertex using the heat method, and visualize the resulting distance field as a soft-selection falloff on the sphere.

The goal is educational correctness and visual clarity, not production-level performance.

---

## 1. Project Goal

Build a complete standalone demo app:

- Windows desktop app.
- DirectX 12 renderer.
- Dear ImGui UI.
- Generated triangulated sphere mesh with many vertices.
- Orbit/zoom camera.
- CPU implementation of the heat method on the triangle mesh.
- Dense Cholesky prefactorization implemented manually, with no external linear solver library.
- Visualization of:
  - geodesic distance field
  - soft-selection weights
  - optional displaced soft-selection preview
  - optional contour/band visualization

The app should let the user clearly see that distance spreads across the surface of the sphere, not through space.

---

## 2. Hard Requirements

Use:

- C++17 or C++20.
- DirectX 12.
- Dear ImGui.
- Windows SDK / DirectXMath are allowed.
- No Eigen.
- No CHOLMOD.
- No SuiteSparse.
- No external math/solver libraries.
- No Dijkstra fallback as the main result.
- No Euclidean-distance fake implementation.

Dear ImGui itself may be vendored normally, using the official Win32 + DX12 backends.

The heat method must be implemented directly.

---

## 3. Expected User Experience

When the app runs, the user sees:

- A triangulated sphere in the viewport.
- Per-vertex colors showing heat-method distance or soft-selection weights.
- A highlighted source vertex.
- Camera controls:
  - orbit around sphere
  - zoom in/out
  - optional pan
- ImGui controls for:
  - sphere longitude segments
  - sphere latitude segments
  - source vertex index
  - source picking mode
  - soft-selection radius
  - falloff power
  - displacement amount
  - visualization mode
  - recompute button
  - rebuild mesh button
  - diagnostic values

The user should be able to change the source vertex and recompute the distance field. The expensive matrix factorizations should be reused after the mesh is built; changing only the source should not rebuild/refactor the matrices.

---

## 4. Suggested UI

Dear ImGui panel: “Heat Method Demo”

Controls:

- Mesh
  - Longitude Segments: default 48, min 12, max 96
  - Latitude Segments: default 24, min 6, max 48
  - Rebuild Mesh

- Source
  - Source Vertex: integer slider/input
  - Pick Source With Mouse: checkbox
  - Recompute Distance

- Heat Method
  - Time Step Scale m: default 1.0
  - Actual timestep t = m * h^2
  - Refactor Matrices button
  - Recompute Using Existing Factors button

- Soft Selection
  - Radius: default around 0.8 on unit sphere
  - Falloff Power: default 2.0
  - Use Smoothstep: checkbox
  - Displacement Amount: default 0.15

- Visualization Mode
  - Distance
  - Distance Bands
  - Soft Weight
  - Displaced Soft Weight
  - Analytic Error

- Diagnostics
  - Vertex count
  - Triangle count
  - Mean edge length h
  - Timestep t
  - Heat matrix factor success/failure
  - Poisson matrix factor success/failure
  - min/max heat u
  - min/max distance phi
  - min/max weight
  - mean absolute error vs analytic sphere distance
  - max absolute error vs analytic sphere distance
  - normalized mean/max error divided by sphere radius or pi

---

## 5. Sphere Mesh

Generate the sphere on CPU.

Use a unit sphere.

Avoid duplicated pole vertices. Use:

- One top pole.
- One bottom pole.
- latitudeSegments - 1 interior rings.
- longitudeSegments vertices per interior ring.

For example:

- longitudeSegments = 48
- latitudeSegments = 24
- vertex count = 2 + (latitudeSegments - 1) * longitudeSegments
- triangle count roughly = 2 * longitudeSegments * (latitudeSegments - 1)

Generate triangles with consistent winding.

Each vertex should store:

```cpp
struct Vertex {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT4 color;
};
```

Also keep CPU-side data:

```cpp
struct Vec3 {
    double x;
    double y;
    double z;
};

struct Tri {
    int a;
    int b;
    int c;
};
```

Keep original sphere positions separate from displayed positions, because displacement preview should not destroy the original geometry.

```cpp
std::vector<Vec3> basePositions;
std::vector<Vec3> displayPositions;
std::vector<Vec3> normals;
std::vector<Tri> triangles;
```

Normals for a unit sphere are normalized positions.

---

## 6. Camera

Implement an orbit camera:

- Target defaults to origin.
- Distance defaults to 3.0.
- Yaw/pitch.
- Mouse drag rotates camera when ImGui does not capture the mouse.
- Mouse wheel zooms.
- Clamp pitch to avoid flipping.
- Use perspective projection.
- Use a depth buffer.

---

## 7. Rendering

Implement a simple DX12 rendering path:

- Swap chain.
- RTV heap.
- DSV heap.
- Command queue/list/allocator.
- Fence synchronization.
- Vertex buffer.
- Index buffer.
- Constant buffer containing MVP matrix.
- Simple root signature.
- Simple pipeline state.

HLSL can be embedded as strings or stored as files.

Vertex shader:

- input position, normal, color
- output clip position, normal, color

Pixel shader:

- use vertex color
- optional simple directional lighting:
  - ambient = 0.35
  - diffuse = max(dot(normal, lightDir), 0) * 0.65
  - finalColor = vertexColor * (ambient + diffuse)

Also draw source vertex visibly:

- Either render a small point/marker at source position.
- Or make source vertex color white/bright.
- Optional: draw a tiny sphere marker using simple generated geometry.

---

## 8. Heat Method Math Overview

Implement the triangle-mesh heat method.

The continuous algorithm is:

1. Integrate heat flow for a short time.
2. Normalize the heat gradient:

```text
X = -grad(u) / |grad(u)|
```

3. Recover distance by solving a Poisson equation.

For this demo use a positive cotangent stiffness matrix K.

Implementation convention:

```text
K = positive cotangent stiffness matrix
M = lumped mass vector
A = heat matrix = M + tK
u = heat field
X = normalized per-face vector field
phi = distance field
```

Do not use the paper’s negative-semidefinite sign convention directly in code. Use the positive stiffness convention because it is easier for Cholesky.

---

## 9. Dense Matrix Representation

Use dense matrices for simplicity.

```cpp
struct DenseMatrix {
    int n = 0;
    std::vector<double> data; // row-major, size n*n

    void Resize(int size);
    double& At(int row, int col);
    double At(int row, int col) const;
    void SetZero();
};
```

Dense is acceptable for this demo. Start with around 1100 vertices. That is large enough to see the effect, small enough for educational dense Cholesky.

---

## 10. Dense Cholesky

Implement a simple LLT Cholesky factorization manually.

```cpp
class DenseCholesky {
public:
    bool Factor(const DenseMatrix& matrix);
    bool Solve(const std::vector<double>& rhs, std::vector<double>& x) const;

private:
    int n = 0;
    std::vector<double> lower; // row-major lower triangular L
};
```

Factorization:

```text
A = L * L^T
```

Solve:

```text
L y = b
L^T x = y
```

Use double precision.

If a diagonal is slightly non-positive due to numerical issues, report failure. For the demo, add a tiny diagonal regularization only where appropriate:

- heat matrix should usually be strictly SPD
- pinned Poisson matrix should be SPD
- regularization may be 1e-8 or 1e-10 if needed, but report it in diagnostics

Do not silently hide serious factorization failure.

---

## 11. Cotangent Stiffness Matrix K

Build positive cotangent stiffness matrix K.

For each triangle with vertices i, j, k and positions p0, p1, p2:

Compute area:

```cpp
Vec3 e01 = p1 - p0;
Vec3 e02 = p2 - p0;
Vec3 cross = Cross(e01, e02);
double doubleArea = Length(cross);
double area = 0.5 * doubleArea;
```

Skip or report degenerate triangles if area is too small.

Cotangent at vertex 0:

```cpp
cot0 = Dot(p1 - p0, p2 - p0) / Length(Cross(p1 - p0, p2 - p0));
```

Cotangent at vertex 1:

```cpp
cot1 = Dot(p2 - p1, p0 - p1) / Length(Cross(p2 - p1, p0 - p1));
```

Cotangent at vertex 2:

```cpp
cot2 = Dot(p0 - p2, p1 - p2) / Length(Cross(p0 - p2, p1 - p2));
```

Each cotangent contributes to the opposite edge.

For edge (1, 2), weight += 0.5 * cot0.  
For edge (2, 0), weight += 0.5 * cot1.  
For edge (0, 1), weight += 0.5 * cot2.

Add edge stiffness:

```cpp
void AddEdge(DenseMatrix& K, int i, int j, double w) {
    K.At(i, i) += w;
    K.At(j, j) += w;
    K.At(i, j) -= w;
    K.At(j, i) -= w;
}
```

Do not clamp negative cotangents by default. Negative cotangents can happen on obtuse triangles. The matrix should still generally work for a reasonable sphere mesh. If Cholesky fails, report diagnostics and optionally add a small diagonal regularizer.

---

## 12. Lumped Mass Vector M

Use barycentric lumped vertex mass.

For each triangle:

```cpp
mass[i] += area / 3.0;
mass[j] += area / 3.0;
mass[k] += area / 3.0;
```

The heat matrix is dense:

```cpp
A = K * t;
A[i][i] += mass[i];
```

More explicitly:

```cpp
heat.At(r, c) = timestep * stiffness.At(r, c);
for each i:
    heat.At(i, i) += mass[i];
```

---

## 13. Mean Edge Length h and Timestep

Compute mean edge length h from unique mesh edges.

Use a set of ordered edge pairs:

```cpp
edge = {min(i, j), max(i, j)}
```

Compute:

```cpp
h = average length of all unique edges
t = m * h * h
```

Default:

```cpp
m = 1.0
```

Expose m in ImGui as “Time Step Scale.”

---

## 14. Heat Solve

Build source vector b.

For single source vertex:

```cpp
b.assign(n, 0.0);
b[source] = 1.0;
```

For future multiple sources, set each source entry to 1.0.

Solve:

```cpp
(M + tK) u = b
```

using the prefactored heat matrix.

Store `u`.

Diagnostics:

- min u
- max u
- check for NaN/Inf

---

## 15. Per-Face Gradient of u

For each triangle, compute the gradient of the piecewise-linear scalar field u.

Given triangle vertices:

```cpp
int i0 = tri.a;
int i1 = tri.b;
int i2 = tri.c;

Vec3 p0 = positions[i0];
Vec3 p1 = positions[i1];
Vec3 p2 = positions[i2];

double u0 = u[i0];
double u1 = u[i1];
double u2 = u[i2];
```

Compute:

```cpp
Vec3 normal = Normalize(Cross(p1 - p0, p2 - p0));
double area = 0.5 * Length(Cross(p1 - p0, p2 - p0));
```

Use the standard triangle gradient formula:

```cpp
Vec3 e0 = p2 - p1; // opposite vertex 0
Vec3 e1 = p0 - p2; // opposite vertex 1
Vec3 e2 = p1 - p0; // opposite vertex 2

Vec3 grad =
    (u0 * Cross(normal, e0) +
     u1 * Cross(normal, e1) +
     u2 * Cross(normal, e2)) / (2.0 * area);
```

Then:

```cpp
Vec3 X;
double len = Length(grad);

if (len > 1e-12) {
    X = -grad / len;
} else {
    X = Vec3{0.0, 0.0, 0.0};
}
```

Store one X vector per face.

---

## 16. Poisson RHS Using Weak Form

Use the weak-form RHS because it avoids sign confusion.

We want to minimize:

```text
integral |grad(phi) - X|^2
```

This gives:

```text
K phi = b
```

where:

```text
b_i = integral X dot grad(lambda_i) dA
```

For each triangle, compute gradients of barycentric basis functions:

```cpp
gradLambda0 = Cross(normal, e0) / (2.0 * area);
gradLambda1 = Cross(normal, e1) / (2.0 * area);
gradLambda2 = Cross(normal, e2) / (2.0 * area);
```

Then add:

```cpp
rhs[i0] += area * Dot(Xface, gradLambda0);
rhs[i1] += area * Dot(Xface, gradLambda1);
rhs[i2] += area * Dot(Xface, gradLambda2);
```

This corresponds to the Poisson recovery step in the heat method but uses the positive stiffness convention.

---

## 17. Poisson Solve and Pinning

The stiffness matrix K is singular on a closed mesh because phi is only determined up to an additive constant.

For this demo, solve by pinning one vertex.

Use fixed pin vertex 0 so that the Poisson factor can be reused across source changes.

Create a copy of K:

```cpp
poisson = stiffness;
poissonRhs = rhs;
```

Apply pin constraint:

```cpp
int pin = 0;

for all j:
    poisson.At(pin, j) = 0.0;
    poisson.At(j, pin) = 0.0;

poisson.At(pin, pin) = 1.0;
poissonRhs[pin] = 0.0;
```

Then factor the pinned Poisson matrix once after mesh build.

Solve:

```cpp
poissonFactor.Solve(poissonRhs, phi);
```

After solve:

```cpp
double minPhi = min(phi);
for each i:
    phi[i] -= minPhi;
```

If distances appear inverted, diagnose the sign of the RHS. Do not replace the algorithm with Euclidean distance. Fix the sign convention.

---

## 18. Analytic Sphere Validation

Because the mesh is a unit sphere, compute exact smooth-sphere geodesic distance from the source for validation:

```cpp
exact[i] = acos(Clamp(Dot(Normalize(p_i), Normalize(p_source)), -1.0, 1.0));
```

This exact distance is along the smooth unit sphere.

Compute:

```cpp
error[i] = abs(phi[i] - exact[i]);
meanError = average(error);
maxError = max(error);
meanRelative = meanError / pi;
maxRelative = maxError / pi;
```

Show these in ImGui.

Also support “Analytic Error” visualization mode.

---

## 19. Soft Selection Weights

Convert distance phi to weight.

```cpp
double x = phi[i] / radius;
double w = Clamp01(1.0 - x);
```

If smoothstep enabled:

```cpp
w = w * w * (3.0 - 2.0 * w);
```

Apply falloff power:

```cpp
w = pow(w, falloffPower);
```

Store weights.

The source should have weight near 1. Vertices outside radius should be 0.

---

## 20. Displacement Preview

For visualization mode “Displaced Soft Weight”:

```cpp
displayPosition[i] = basePosition[i] + normals[i] * weight[i] * displacementAmount;
```

For other modes:

```cpp
displayPosition[i] = basePosition[i];
```

Then update the DX12 vertex buffer.

Do not permanently edit the mesh.

---

## 21. Color Mapping

Implement clear color modes.

Distance mode:
- Normalize phi by maxPhi.
- Use a gradient from near-source bright/warm to far dark/cool.
- Source should be obvious.

Distance Bands mode:
- Use phi to create contour rings:

```cpp
double bands = 0.5 + 0.5 * cos(phi[i] * bandFrequency);
```

- Combine bands with a base distance gradient.

Soft Weight mode:
- weight 1 = bright warm color
- weight 0 = dark cool/gray color

Analytic Error mode:
- low error = dark/blue
- high error = bright/red

Exact colors are up to you, but the result must be visually obvious.

---

## 22. Mouse Source Picking

Implement at least one reliable source-selection method.

Required:
- Source vertex index input in ImGui.

Preferred:
- Ctrl + left click chooses nearest visible/projected vertex.

Simpler picking approach:
- Convert each vertex world position to clip/screen space using current MVP.
- Ignore vertices behind camera.
- Find nearest screen-space vertex to mouse.
- If distance is less than threshold, e.g. 15 pixels, set it as source.
- Recompute distance.

This is acceptable for a demo.

Do not spend too much time implementing perfect triangle picking.

---

## 23. Rebuild and Recompute Rules

When mesh resolution changes:
- Rebuild sphere mesh.
- Rebuild mass vector.
- Rebuild stiffness matrix.
- Recompute h and timestep.
- Build heat matrix.
- Build pinned Poisson matrix.
- Factor heat matrix.
- Factor Poisson matrix.
- Solve current source.

When only source changes:
- Reuse heat factor.
- Reuse Poisson factor if pin is fixed to vertex 0.
- Rebuild source RHS.
- Solve heat.
- Recompute gradient X.
- Recompute Poisson RHS.
- Solve Poisson.
- Shift phi by min.
- Recompute weights.
- Update colors.

When only radius/falloff/displacement changes:
- Do not rerun heat method.
- Recompute weights/colors/displacement only.
- Update vertex buffer.

When timestep scale m changes:
- Rebuild heat matrix.
- Refactor heat matrix.
- Recompute heat/distance.

---

## 24. File Organization

Use a clear educational file layout.

Suggested:

```text
/HeatMethodDx12Demo
    HeatMethodDx12Demo.sln
    /src
        main.cpp
        App.h
        App.cpp
        RendererD3D12.h
        RendererD3D12.cpp
        Camera.h
        Camera.cpp
        Mesh.h
        Mesh.cpp
        HeatMethod.h
        HeatMethod.cpp
        DenseMatrix.h
        DenseMatrix.cpp
        DenseCholesky.h
        DenseCholesky.cpp
        MathTypes.h
        MathTypes.cpp
        Shaders.hlsl
    /third_party
        /imgui
    /docs
        HEAT_METHOD_DEMO_NOTES.md
        BUILD.md
```

If using a Visual Studio solution, document exact Visual Studio version and required SDK.

---

## 25. Documentation

Create `/docs/HEAT_METHOD_DEMO_NOTES.md`.

Explain:

- What the heat method computes.
- Why the demo uses a sphere.
- What matrices are built.
- Why dense Cholesky is used instead of sparse Cholesky.
- Why the heat matrix is `M + tK`.
- Why the Poisson matrix needs a pinned vertex.
- Why the final distance is shifted by subtracting min.
- How soft-selection weights are derived from phi.
- How to compare against analytic sphere distance.

Create `/docs/BUILD.md`.

Include:
- prerequisites
- build steps
- run steps
- known limitations

---

## 26. Known Limitations to State in Docs

Mention these clearly:

- Dense Cholesky is for demo/education only.
- Real production implementation should use sparse matrices and sparse Cholesky or another sparse prefactorized solver.
- UV sphere triangles near poles are not ideal but sufficient for the demo.
- Heat-method distance approximates smooth geodesic distance; exact result improves with mesh refinement.
- Source selection is vertex-based, not arbitrary point-on-triangle.
- The demo uses a closed surface, so no boundary conditions are implemented.

---

## 27. Code Style

Prefer simple, readable C++.

Use clear names.

Use comments around the heat method math.

Use K&R-style braces:

```cpp
if (condition) {
    DoThing();
} else {
    DoOtherThing();
}
```

Use braces for all if statements.

Keep short one-line braced if statements on one line when readable:

```cpp
if (value < 0.0) { value = 0.0; }
```

Avoid cryptic names for important variables. Short names are okay when standard:

- `u` for heat
- `phi` for distance
- `K` for stiffness
- `M` for mass
- `rhs` for right-hand side
- `grad` for gradient

---

## 28. Debugging Requirements

Add assertions or checks for:

- matrix sizes
- source vertex range
- NaN/Inf in u
- NaN/Inf in phi
- NaN/Inf in weights
- Cholesky failure
- degenerate triangle count
- row sum of stiffness matrix near zero before pinning
- matrix symmetry approximately true

Show important failures in ImGui and OutputDebugString.

---

## 29. Acceptance Criteria

The task is complete when:

1. The app builds and runs on Windows.
2. A sphere is visible in a DX12 viewport.
3. Dear ImGui controls are visible.
4. The camera can orbit and zoom.
5. The user can select or enter a source vertex.
6. The heat method computes a distance field using:
   - cotangent stiffness matrix
   - lumped mass matrix
   - dense Cholesky heat solve
   - per-face heat gradient
   - normalized vector field
   - weak-form Poisson RHS
   - dense Cholesky Poisson solve
7. The sphere displays a recognizable geodesic distance/soft-selection field.
8. Soft-selection radius and falloff update the visualization without rerunning the heat solve.
9. Displacement preview moves vertices outward according to soft-selection weight.
10. Analytic sphere error is computed and displayed.
11. Documentation explains how to build, run, and understand the demo.

---

## 30. Implementation Priority

Implement in this order:

1. Basic Windows + DX12 + ImGui app.
2. Orbit camera.
3. Generated sphere mesh rendering with vertex colors.
4. DenseMatrix and DenseCholesky.
5. Cotangent stiffness and mass matrix build.
6. Heat solve.
7. Face gradients and normalized X.
8. Poisson RHS and Poisson solve.
9. Distance visualization.
10. Soft-selection weights.
11. Displacement preview.
12. Mouse picking.
13. Analytic error visualization.
14. Documentation.

Do not jump to sparse solvers. Do not add unrelated engine systems.

---

## 31. Critical Math Sign Convention Note

Use the positive stiffness matrix convention:

```text
K is positive semidefinite
heat solve:    (M + tK) u = source
Poisson solve: K phi = rhs
rhs_i = sum over faces area * dot(X_face, grad(lambda_i))
X_face = -normalize(grad u)
```

After solving phi:

```text
phi = phi - min(phi)
```

This should produce distance increasing away from the source.

If the result is inverted or wrong, debug the sign of:
- triangle winding
- gradient formula
- X = -normalize(grad u)
- Poisson RHS

Do not replace the method with graph distance or Euclidean distance.

---

## 32. Optional Nice-to-Have Features

Only after the core demo works:

- Wireframe overlay.
- Toggle lighting.
- Display selected source as a small marker.
- Show contour isolines more clearly.
- Multiple source vertices.
- Random source button.
- Reset camera button.
- Export screenshot.
- Compare against Euclidean distance mode for educational contrast.

---

## 33. Final Response Expected From Codex

When finished, summarize:

- files created
- how to build
- how to run
- what the heat method implementation does
- any limitations
- any known issues
- whether the project was compiled/tested

---

## Additional instruction for Codex

Include the PDF in `/docs/Geodesics_in_Heat.pdf` and read Section 3, especially 3.1, 3.2.1, and 3.2.4, before implementing the math core.
