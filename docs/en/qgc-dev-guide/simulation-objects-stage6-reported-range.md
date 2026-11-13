# Stage 6 — Read-only reported slant range in missile markers

This focused change uses the **existing** `kernel.qgc.simobjects` UDP JSON v1
`state.engagement` fields. It introduces no protocol revision and no new
MAVLink vehicles, control commands, or GCS-computed distance/engagement estimate.

## Fields

The existing decoder continues to validate `engagement.slant_range_m` as
nonnegative and finite, and `engagement.range_valid` as a boolean. When
`range_valid: true`, the wire contract still requires
`engagement.range_sim_time_ns` to be a decimal string and
`slant_range_m` to be present as a JSON number.

After validation, the decoder now copies `slant_range_m` and `range_valid`
into the packet model; `SimulationObjectManager` persists them on each
accepted object `state` update. Each `SimulationObject` exposes read-only,
NOTIFY-backed `slantRangeM` and `rangeValid` QML properties.

- Valid range: a black range badge below the missile ID/status text, formatted
  in whole meters with spaces for thousands (`1 234 m`). Exactly zero
  remains `0 m` when valid.
- Invalid range: no badge (even if the packet contains a numeric but
  `range_valid: false` value).
- Missing or null range: no badge and `slantRangeM` is NaN, so an old valid
  distance cannot leak into later states.
- The ID/status remains beside the missile as subtle text, without the old
  bordered/background box. Marker position, heading and lifecycle behavior
  are otherwise unchanged.
- The same `SimulationObjectMarker.qml` is used by QtLocation and GeoMap.
  GeoMap's existing icon-label visibility across 2D/3D modes is unchanged.

No LOS endpoint matching, range calculation, guidance or simulation-side
changes are implemented by this *reported-range* increment.

## Example valid wire fragment

Add this to a valid existing missile `state` packet, without changing its
envelope or other fields:

```json
"engagement": {
  "slant_range_m": 1234.4,
  "range_valid": true,
  "range_sim_time_ns": "250000000"
}
```

The range timestamp is the authoritative simulation timestamp as a decimal
string, not a JSON floating-point number.

## Windows acceptance

1. Switch to `feat/simobjects-stage6-reported-range` and build via
   the existing **Ctrl+Shift+B** VS Code task.
2. Run the existing regression test plus the new range tests:

   ```powershell
   ctest --preset Windows-debug -R SimulationObjectTest --output-on-failure
   ```

3. Launch with `run-qgc.ps1`, keep the simulation receiver enabled, then
   start the existing *missiles-only* fixture with its opt-in synthetic badge:

   ```powershell
   python .\tools\simobjects\phase1_publisher.py --duration 120 --rate 30 --center-lat 43.41 --center-lon 39.94 --demo-range-m 1234.4
   ```

   Expected: `1 234 m` appears inside the black range pill on each visible
   missile. The fixture field is deliberately synthetic and fixed; it is
   **not** derived from any live NPC or missile-target geometry.
4. Run the same fixture **without** `--demo-range-m`. Its default
   `range_valid: false` produces no range pill; ID/status text remains.
5. With live kernel-sim, check that its actual reported range values replace
   the badge when valid and disappear when invalid or missing.
6. Verify object completion and stale retention, ordinary MAVLink aircraft,
   and both QtLocation and GeoMap map modes.

No Windows build or GUI acceptance test has run from the GitHub connector.
Keep the PR in draft until local validation succeeds.
