# Map trail camera-alignment fix (QtLocation)

**Stacked on:** `feat/simobjects-stage6-visual-polish` (PR #8).
**Scope:** QtLocation 2D Vehicle and Simulation Object trails. Does **not**
change the LOS implementation, GeoMap 2D/3D renderer, historical sample
timestamps, flight-distance accounting, or UDP/MAVLink protocols.

## Symptom and cause

A free-floating screen-space `Canvas` in a `Repeater` manually projected
coordinates using `Map.fromCoordinate`. During pan, wheel zoom or animated
camera changes, the canvas could be repainted at a different time from
QtLocation's native geographic markers/LOS, leaving trails offset from icons.

## Fix

- The shared timestamped `MapTrailHistory` exposes a read-only
  `renderAnchor`: the **last retained geographic sample** or invalid when
  empty. Appending and expiring history update the existing `changed`
  notification. Tests cover append, rolling expiration, empty, clear and
  resumption.
- Each QtLocation trail is now a **native `MapQuickItem`**, instantiated
  by `MapItemView` instead of an ordinary `Repeater`.
  Its geographic `coordinate` is `MapTrailHistory.renderAnchor`.
- Its `sourceItem` contains the same Canvas renderer, now painting
  **relative** offsets:
  `local(point) = viewportProjection(point) - viewportProjection(anchor) + canvasCenter`.
  Consequently, a map pan translates both the marker and its geographically
  anchored trail with QtLocation's map renderer **even before a new
  Canvas repaint**. Zoom and bearing still trigger geometry reprojection.
- The bounded Canvas spans two viewport dimensions around the geographic
  anchor so line segments visible on the opposite side of the screen are
  not cut off when the tracked object approaches a viewport edge.
- All existing 2D trail styles (solid/dashed/dotted/fading/none), colors,
  opacity, independent category settings, 10-second default rolling
  lifetime, stationary expiration and multiple histories remain unchanged.
- GeoMap continues to use `FlightPathGeometry`, a terrain-aware
  scene-anchored GPU ribbon. The source code did not indicate the same
  viewport-space mismatch in that renderer, so this patch does not
  modify GeoMap or LOS.

## Windows acceptance

1. Quit `QGroundControl.exe` before rebuilding (otherwise Windows may
   fail with `LNK1168`).
2. Switch to `fix/map-trail-camera-alignment` and build using
   VS Code **Ctrl+Shift+B**.
3. Run:

   ```powershell
   ctest --preset Windows-debug -R "(MapTrailHistoryTest|FlightPathGeometryTest|SimulationObjectTest|TrajectoryPointsTest)" --output-on-failure
   .\run-qgc.ps1
   ```

4. Run your **real kernel-sim** with both normal MAVLink aircraft and
   missiles; the LOS must stay unchanged.
5. In the QtLocation Fly View, switch both trail groups to 10 s, then
   test Solid, Dashed, Dotted and Fading independently.
6. With moving aircraft and missiles, drag the map rapidly horizontally
   and vertically, zoom in/out with mouse wheel and +/- controls, and
   toggle picture-in-picture. Check that trails remain geographically
   attached to their paths/markers without moving sideways.
7. Stop simulation telemetry and verify existing trailing samples expire
   after 10 s even while objects are stationary. Check that changing
   lifetime/style, object deletion and active Vehicle switching work.
8. Toggle into GeoMap 2D/3D and check its existing terrain-aware trails,
   LOS and icons for regressions.

## Performance gate

The Qt 6 `Canvas.Image` backend uses image uploads. The geographically
anchored source canvas can be up to **2× viewport width and height**, so
it may consume up to 4× the image area of the previous screen-space
canvas per moving object. This is a correctness-first fix and **needs
profiling with 4K screens and up to 10 objects** before production use.
If that becomes a bottleneck, migrate styled QtLocation trails to a
custom scene-graph renderer driven by the same `MapTrailHistory` model.
Avoid per-sample QML item creation and avoid reintroducing a viewport
anchoring bug.

**No Windows build or visual acceptance has run from GitHub access.**
This branch remains draft pending local validation.
