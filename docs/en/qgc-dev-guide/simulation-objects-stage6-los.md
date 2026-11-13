# Stage 6 — Read-only simulator-to-aircraft LOS overlay

**Dependency:** `feat/simobjects-stage6-reported-range` (PR #6).
This branch adds **LOS visualization and white ID/status labels**. It does not
add interpolation, guidance, interception estimation, control commands or
a new wire protocol.

## Why no line appeared in the Stage 6 range test

PR #6 added only the reported range badge and ID/status typography; it did
not contain LOS drawing code. The default `phase1_publisher.py` mode emits
missiles only and intentionally removes their `target` objects, so the GCS
has no trustworthy endpoint to draw.

`--demo-range-m 1234.4` injects a fixed **display-only range** value; it does
not create a target binding or calculate aircraft distance.

## Required fields (already valid UDP JSON v1)

A missile `state` packet may include `target.mavlink`:

```json
"target": {
  "npc_id": "npc-11",
  "generation": "1",
  "valid": true,
  "mavlink": {
    "source_key": "qgc-fixture-npc",
    "sysid": 11,
    "compid": 1
  }
}
```

The existing packet decoder validation is unchanged. `source_key` is
persisted, as are `targetSysId`, `targetCompId`, and
`targetBindingValid`. The GCS resolves only explicit valid bindings:
**MAVLink system ID and default component ID must match a connected Vehicle**.
It does **not** guess by the textual NPC name, and no new Vehicle is created.

QGC's current `Vehicle` model does not expose the simulator-specific
`source_key` for identity comparison. It is preserved and required to be
nonempty, but cannot yet be cross-checked against the MAVLink vehicle
connection. If distinct MAVLink connections reuse an identical sysid and
compid, source_key alone cannot disambiguate them. Such integration should
ensure unique MAVLink IDs or add a trustworthy source-key mapping later.

## Visual behavior

- The missile ID/status becomes **white text** (no ID background box).
- The existing black badge continues to display only the simulator-reported
  range when valid, with grouped meters (e.g. `1 234 m`).
- QtLocation draws a low-opacity light-blue 2 px
  `MapPolyline` between the missile and its live bound Vehicle.
  QtLocation's native `MapPolyline` offers only width and color, so it is
  a **continuous** line in this engine.
- GeoMap reuses the existing `FlightPathGeometry` ribbon and shader for
  a translucent **dashed** 2 px line in both 2D and 3D. It retains the
  terrain/home AMSL correction for the MAVLink target and ground-clamps
  missile heights with unavailable datum conversion.
- The line disappears as soon as the binding, object position, live aircraft
  position, or active lifecycle becomes invalid. It follows both endpoints
  as their respective coordinate signals change.
- No line is drawn for `target.mavlink` absent/null, or for a mere
  `target.npc_id` string.

## Windows 10 acceptance checklist

1. Close QGC first to avoid `LNK1168` (executable locked by the running app).
2. `git fetch origin`, switch to `feat/simobjects-stage6-los`.
3. Rebuild using the existing VS Code **Ctrl+Shift+B** task.
4. Run:

   ```powershell
   ctest --preset Windows-debug -R "(SimulationObjectTest|FlightPathGeometryTest)" --output-on-failure
   .\run-qgc.ps1
   ```

5. Start the *real kernel-sim* telemetry publisher and existing MAVLink NPCs.
   The missile `state` must include the optional `target.mavlink`
   binding with the aircraft's live `sysid`/`compid`.
6. Verify: missile ID/status is white; reported range badge unchanged;
   cyan LOS line appears only when binding and both coordinates are valid.
   Test position changes, object completion, missing/invalid bindings,
   disconnection and session reset in both QtLocation and GeoMap.
7. A local display-only fixture may alternatively be started with
   `--with-fixture-npcs` in a controlled isolated test, but **do not** run
   it alongside real MAVLink NPCs because it emits additional synthetic
   aircraft and may collide with system IDs.

No local Windows compilation, Qt Quick UI test, or live simulator acceptance
has run through the GitHub connector. Keep the PR draft pending verification.
