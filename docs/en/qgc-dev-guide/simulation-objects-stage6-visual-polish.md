# Stage 6.1 — LOS layering and numeric missile labels

This small visualization-only phase builds on `feat/simobjects-stage6-los`.
It preserves the existing UDP JSON v1 decoder, MAVLink integration, reported
range, simulated object identities, and lifecycle logic.

## Changes

### Label

The `SimulationObjectMarker.qml` label is now only the numeric suffix of
a canonical `missile-N` or `missile_N` object ID:

- `missile-1` → `1`
- `missile-12` → `12`
- `missile-002` → `002`

Nonmatching object IDs display verbatim rather than producing a misleading or
empty number. The full ID is still retained by `SimulationObjectManager`
and used for packet identity/session tracking.

The former `· ?` heading annotation and parenthetical lifecycle suffix
are removed from the marker **text**. The missile artwork still changes
color by lifecycle. The existing black pill continues to contain the
**reported slant range** supplied by the simulator and remains hidden when
invalid.

### LOS stacking

- **QtLocation:** the LOS `MapItemView` is created before the Vehicle and
  Simulation Object marker views, and its map-item `z` is explicitly
  `zOrderTrajectoryLines` (below `zOrderVehicles` and missile markers).
- **GeoMap:** LOS components are created before the missile marker
  components. Its 2D overlay has a lower `z` than the marker.
- **GeoMap 3D:** QML item `z` alone cannot reliably order independent
  transparent 3D models. The LOS ribbon therefore begins just outside
  the missile's approximate screen-space radius, computed with the same
  camera scale used for marker sizing. This is a **rendering-only inset**;
  neither endpoint in telemetry, the object model nor the
  simulator-reported range is modified.
- If the projected LOS is shorter than the marker radius, the GeoMap 3D
  overlay suppresses the line rather than drawing across the icon.
  Vertical-only segments are handled through the same 3D length
  computation.

## Windows acceptance

1. Close QGC before the build to avoid `LNK1168` from a locked executable.
2. Switch to branch `feat/simobjects-stage6-visual-polish`.
3. Build with the VS Code **Ctrl+Shift+B** task.
4. Run:

   ```powershell
   ctest --preset Windows-debug -R "(SimulationObjectTest|FlightPathGeometryTest)" --output-on-failure
   .\run-qgc.ps1
   ```

5. Start the **real kernel-sim** simulation telemetry and normal MAVLink NPCs.
   Ensure the missile state includes `target.mavlink` so LOS is drawable.
6. Check `missile-1` shows the white text `1` only, `missile-10`
   shows `10`, and the black range pill remains separate.
7. Verify the LOS line sits **behind the missile silhouette** when passing
   through its position in QtLocation 2D and GeoMap 2D/3D. Pan, zoom and
   rotate the 3D camera; the line must continue to follow the endpoints
   without reappearing on top of the source marker.
8. Verify stale/completed/missing binding still removes the LOS; two
   independent missile instances do not transfer labels or lines.

No Windows build, CTest or GUI acceptance was executed through the GitHub
connector. Keep the PR in draft until these checks pass.
