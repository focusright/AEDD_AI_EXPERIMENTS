# Heat Method Demo Notes

This demo implements the heat method described by Crane, Weischedel, and Wardetzky in "Geodesics in Heat." The paper is included beside this file as `Geodesics_in_Heat.pdf`.

## What the Heat Method Computes

The heat method approximates geodesic distance on a surface. Rather than walking through 3D space, it uses operators built from the triangle mesh so distance spreads intrinsically across the surface.

For a source vertex, the demo performs the three core steps from the paper:

1. Integrate heat flow for a short time.
2. Normalize and negate the heat gradient to get a unit vector field pointing away from the source.
3. Solve a Poisson equation to recover a scalar distance potential.

The implementation uses the positive cotangent stiffness convention:

```text
K is positive semidefinite
heat solve:    (M + tK) u = source
vector field:  X = -normalize(grad u)
Poisson solve: K phi = rhs
rhs_i = sum over faces area * dot(X_face, grad(lambda_i))
```

After solving, `phi` is shifted by subtracting its minimum value so the smallest distance is zero.

## Why a Sphere

The generated unit sphere is simple, closed, and has an analytic smooth geodesic distance:

```text
exact_i = acos(dot(normalize(p_i), normalize(p_source)))
```

That lets the demo show mean and maximum error against a known reference. The distance visualization should form bands that travel around the sphere surface, not through the sphere volume.

## Mesh and Operators

The mesh is a CPU-generated UV sphere with one top pole, one bottom pole, and no duplicated pole vertices. Interior latitude rings contain `longitudeSegments` vertices.

For every triangle, the code builds:

- barycentric lumped mass vector `M`
- positive cotangent stiffness matrix `K`
- per-face heat gradient
- weak-form Poisson right-hand side

The cotangent contribution for each triangle is added to the edge opposite the angle, using:

```text
K_ii += w
K_jj += w
K_ij -= w
K_ji -= w
```

## Why Dense Cholesky

The demo uses dense matrices and a manually implemented LLT Cholesky factorization because the goal is educational clarity. A dense solver keeps the matrix assembly and factorization visible in a compact codebase.

This is not a production choice. Real geometry tools should use sparse matrices and sparse prefactorized solvers.

## Heat Matrix

The heat step uses backward Euler:

```text
(M + tK) u = source
```

`M` is a lumped mass vector, `K` is the positive cotangent stiffness matrix, and `t = m h^2`, where `h` is the mean unique edge length and `m` is the user-controlled time step scale. The default `m = 1` follows the paper's practical time-step recommendation.

The heat matrix is prefactored after mesh build. Changing only the source vertex reuses the factorization.

## Poisson Pinning

On a closed mesh, the stiffness matrix has a constant nullspace, so `K phi = rhs` only determines `phi` up to an additive constant. The demo pins vertex 0 by replacing its row and column with an identity constraint. This makes the Poisson matrix symmetric positive definite and lets the Poisson factor be reused across source changes.

After solving, the code subtracts the minimum value from all distances. This removes the arbitrary offset introduced by the pin.

## Soft Selection

Soft-selection weights are derived from heat-method distance:

```text
x = phi / radius
w = clamp(1 - x, 0, 1)
```

If smoothstep is enabled:

```text
w = w*w*(3 - 2*w)
```

Then the falloff power is applied:

```text
w = pow(w, falloffPower)
```

In displaced mode, each displayed vertex is moved outward along its sphere normal by:

```text
normal * weight * displacementAmount
```

The original sphere positions are kept separate, so the preview never permanently edits the mesh.

## Analytic Validation

Because the sphere radius is 1, the exact smooth-sphere distance is the central angle in radians. The demo computes:

```text
error_i = abs(phi_i - exact_i)
meanRelative = meanError / pi
maxRelative = maxError / pi
```

The "Analytic Error" visualization maps low error to dark blue and high error to red.

## Known Limitations

- Dense Cholesky is for demo and education only.
- A real production implementation should use sparse matrices and sparse Cholesky or another sparse prefactorized solver.
- UV sphere triangles near the poles are not ideal, but they are sufficient for this visualization.
- Heat-method distance approximates smooth geodesic distance; results improve with mesh refinement.
- Source selection is vertex-based, not arbitrary point-on-triangle.
- The demo uses a closed surface, so no boundary conditions are implemented.
- The UI blocks while dense matrix factorizations run; this keeps the implementation simple and transparent.
