# Map trails — eliminate per-telemetry trail jitter

**Base:** `perf/map-trails-scenegraph` (PR #10).
This patch changes only QtLocation trail anchor management; no changes to
`kernel-sim`, UDP messages, LOS, labels, vehicle telemetry, or GeoMap 3D.

## Root cause

The QtLocation trail scene-graph is rendered in a native geographic
`MapQuickItem` whose `coordinate` previously followed the **latest sample**.
Every accepted new point shifted the entire MapQuickItem immediately.
However, `MapTrailSceneGraph.setProjectedPath()` runs on a coalescing 33 ms
timer. For one or more frames, the old geometry was displayed relative to
the **new** geographic anchor, briefly moving all existing trail segments.
The mesh then caught up, creating visible shaking at telemetry frequency.

## Fix

- `MapTrailHistory.renderAnchor` is a **stable** geographic coordinate:
  initialized from the first valid sample, preserved on every additional
  point and rolling-window pruning, and cleared only when history empties
  or the object is explicitly cleared.
- A dedicated `renderAnchorChanged` signal avoids unnecessary QML
  map-coordinate updates from each history sample.
- `rebaseToLatest()` is explicit, not automatic on append. The QtLocation
  renderer calls it **inside its synchronous geometry reproject** only
  when the newest sample is more than 75% of the viewport width or height
  from the current projected anchor. The anchor and projected vertices
  are then updated in the same GUI-thread operation, rather than
  changing the anchor before the 33 ms projection timer fires.
- The `MapTrailSceneGraph` item's **logical** bounds are 4x viewport width
  and height, centered on the geographic anchor, to leave margin for
  a slowly moving trail and prevent clipping when rebasing. This item
  holds GPU triangles only, **not a 4x image/texture Canvas**.
- The same bounded 2,048-sample history, monotonic 200 ms expiration,
  per-category styles, fading, color and visibility controls are retained.

## Regression coverage

`MapTrailHistoryTest::_renderAnchor` now checks that:
- the first point initializes the anchor exactly once;
- multiple new position samples do **not** move it or re-emit the anchor
  signal;
- pruning the original sample does not move the anchor;
- an explicit rebase relocates the anchor to the newest sample exactly
  once and further appends do not change it;
- expiry of the last sample or explicit clear invalidates the anchor;
- the next motion history creates a fresh anchor.

## Windows acceptance

1. Close `QGroundControl.exe` before rebuilding to avoid `LNK1168`.
2. Fetch and switch to `fix/map-trails-update-jitter`, then press
   **Ctrl+Shift+B** in VS Code.
3. Run:

   ```powershell
   ctest --preset Windows-debug -R "(MapTrailHistoryTest|FlightPathGeometryTest|TrajectoryPointsTest|SimulationObjectTest)" --output-on-failure
   .\run-qgc.ps1
   ```

4. With live kernel-sim missiles plus MAVLink UAVs, confirm that **existing
   trail segments do not jump at the rhythm of incoming telemetry**.
5. Try 1/10/60 s and Unlimited, Solid/Dashed/Dotted/Fading, stationary
   expiration, and multiple simultaneous aircraft/missiles. Monitor
   re-anchoring after several screens of movement at high zoom, especially
   at high missile velocity.
6. Pan and zoom continuously, and use vehicle-follow mode: marker and
   trail positions must remain geographically aligned. Check LOS is
   unaffected and GeoMap 2D/3D paths continue to work.
7. Keep map interaction responsiveness comparable to PR #10; this patch
   does not reintroduce Canvas or any QImage buffers.

**Build and on-map acceptance have not run through the GitHub connector.**
Keep this PR in draft until Windows tests and visuals pass.
