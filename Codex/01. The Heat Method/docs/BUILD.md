# Build and Run

## Prerequisites

- Windows 10 or newer.
- Visual Studio 2022 Community or newer with the C++ desktop workload.
- Windows SDK.
- A DirectX 12 capable GPU or WARP fallback.

The project uses a plain Visual Studio solution and batch-file build path.

## Build From Command Line

Open a normal terminal in the project directory and run:

```bat
build_vs2022.bat
```

The batch file clears the inherited `PATH` variable before calling `vcvars64.bat`. This avoids an MSBuild issue in some shells where both `PATH` and `Path` exist and the C++ compiler task fails with a duplicate environment key.

The Release executable is written to:

```text
x64\Release\HeatMethodDx12Demo.exe
```

## Build From Visual Studio

Open:

```text
HeatMethodDx12Demo.sln
```

Then choose:

```text
Configuration: Release
Platform: x64
```

Build the solution normally.

## Run

Run:

```text
x64\Release\HeatMethodDx12Demo.exe
```

On startup the app builds the selected mesh, prefactors the heat and Poisson matrices, computes distance from the current source vertex, and opens a DirectX 12 window with an ImGui control panel.

## Controls

- Drag with the left or right mouse button to orbit the camera.
- Use the mouse wheel to zoom.
- Resize or maximize the window to resize the DirectX 12 viewport.
- Choose Sphere, U-Folded Plane, Swiss Roll, or Bunny / Suzanne from "Mesh Type".
- Edit "Source Vertex" or press "Recompute Distance" to change the source.
- Enable "Pick Source With Mouse" and Ctrl-left-click near a visible vertex to select it.
- Change soft-selection radius, falloff, smoothstep, and displacement without rerunning the heat solve.
- Change the time step scale to rebuild the heat matrix and recompute distance.
- Change mesh resolution and press "Rebuild Mesh" to rebuild all operators and factors.

## Troubleshooting

- If command-line MSBuild fails with a duplicate `PATH` or `Path` environment key, use `build_vs2022.bat` instead of invoking MSBuild directly.
- If matrix factorization fails at very high or unusual mesh settings, lower the mesh resolution and rebuild.
- Dense Cholesky is intentionally slow compared with a sparse implementation. The default mesh is chosen to keep the demo understandable while still showing the geodesic field clearly.

## Known Limitations

- Dense Cholesky is for demo and education only.
- Real production code should use sparse matrices and sparse Cholesky or another sparse prefactorized solver.
- UV sphere triangles near poles are not ideal but sufficient for the demo.
- Heat-method distance approximates smooth geodesic distance; exactness improves with mesh refinement.
- Source selection is vertex-based, not arbitrary point-on-triangle.
- The U-folded plane and Swiss roll are open meshes that use the natural boundary behavior of the same weak-form solve.
