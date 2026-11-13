# Map trails — QtLocation scene-graph performance fix

**Base:** `fix/map-trail-camera-alignment` (PR #9).

## Why map drag/zoom became slow

The original `MapTrailCanvas.qml` allocated a `Canvas.Image` surface of
**2× map width × 2× map height** for *every* Vehicle and Simulation Object.
Because Qt 6 Canvas is backed by images, camera changes could cause
full-surface CPU repaints and texture uploads. Ten 1920×1080 trail Canvases
needed about 332 MiB of raw 32-bit RGBA pixels before renderer overhead.

This branch removes that image-backed path entirely.

## Implementation

- `MapTrailSceneGraph` is a new `QQuickItem::ItemHasContents` renderer;
  `updatePaintNode()` manages one `QSGGeometryNode`, a
  `QSGVertexColorMaterial`, and triangle geometry (no QImage or Canvas).
- The existing geo-anchored `MapQuickItem` remains, so **pure panning moves
  its scene-graph geometry with the map without any reprojection or
  per-object image repaint**.
- Zoom/tilt/bearing and viewport resizing coalesce geometry reprojections
  into at most one per 33 ms per object. Trail history append/expiration
  schedules the same bounded refresh.
- Per-sample alpha is uploaded as *premultiplied* vertex color to render
  fading segments with the GPU. Fading alpha is refreshed every 200 ms via
  the existing monotonic history timer, without reprojecting coordinates.
- **Solid** and **fading** use one GPU quad per path segment.
  **Dashed** produces a continuous screen-space 9 px-on/6 px-off pattern;
  **dotted** produces round 8-slice discs at consistent spacing.
  Geometry construction uses projected positions, never deletes trajectory
  samples to create dashed/dotted effects.
- Every tracked object still has its own `MapTrailHistory` and scene-graph
  item. The underlying history keeps its 2,048-sample bound and the
  10-second expiration even when telemetry stops.
- Screen geometry is bounded to **4,096 primitives per object**. For
  extreme zoom or unlimited histories, this can truncate the visible
  decoration, especially dotted/dashed styles. All timestamped samples
  stay intact. This is a rendering safety cap, not a history data limit.
- `QSGVertexColorMaterial` requires the normal Qt Quick scene-graph
  backend, which is how the current Qt 6 QGC application is expected to run.

No changes to flight-distance telemetry, Vehicle/NPC/MAVLink identity,
SimulationObject reception, LOS, target binding, or GeoMap 3D ribbons.

## Files and validation

- `src/FlightMap/MapTrailSceneGraph.h/.cc`: new GPU renderer.
- `src/FlyView/MapTrailCanvas.qml`: keeps the existing component name
  but now uses a `MapTrailSceneGraph` sourceItem instead of `Canvas`.
- `src/FlightMap/CMakeLists.txt`: adds C++ sources and Qt Quick link.
- `src/QGCApplication.cc`: explicitly registers `MapTrailSceneGraph`
  in `QGroundControl.FlightMap`, before the QML engine loads (necessary
  because this is a static QML module with `NO_PLUGIN`).
- `test/Vehicle/MapTrailHistoryTest.cc/.h`: C++ tests for solid,
  fading, dashed and dotted path tessellation, invalid projection
  splitting and primitive bounds, plus QML type registration.

### Windows 10 acceptance

1. Close QGC first to avoid `LNK1168`.
2. Fetch and switch to `perf/map-trails-scenegraph`.
3. **Ctrl+Shift+B** in VS Code, ensuring CMake regenerates.
4. Run:

   ```powershell
   ctest --preset Windows-debug -R "(MapTrailHistoryTest|FlightPathGeometryTest|TrajectoryPointsTest|SimulationObjectTest)" --output-on-failure
   .\run-qgc.ps1
   ```

5. With real kernel-sim / 10 moving missiles plus MAVLink UAVs:
   rapidly pan and zoom the **QtLocation 2D** map, using +/- and wheel.
   Check that the lines remain anchored and motion stays responsive.
6. Change both trail groups independently and verify:
   Solid, Dashed, Dotted, Fading, None; widths, colors and opacity;
   finite lifetime 1/3/10 s and Unlimited; stationary expiration.
7. Verify LOS, numeric missile IDs and reported-range badge are
   unaffected; switch between QtLocation and GeoMap 2D/3D.
8. Compare **CPU, GPU utilization and memory** against the preceding
   `fix/map-trail-camera-alignment` build at the same object count, map
   zoom and viewport resolution. Profile with 1080p and 4K if possible.

### Known limitations and review gates

The previous Canvas and its 4×-viewport image are gone, but mesh rebuilds
still cost CPU when projected positions change, and the QML
`Map.fromCoordinate()` loop still runs on zoom/geometry refresh. The
4,096-primitive cap bounds worst-case uploads. This should substantially
improve responsiveness, but **no FPS or speedup claim can be made before
Windows profiling**. For very long unlimited histories in extreme zoom,
dash/dot patterns may reach the rendering cap before all history is drawn.
The 3D GeoMap renderer is unchanged.

The GitHub connector cannot run a local Windows C++/QML build or GUI tests,
so this remains a draft PR until the user completes validation.
