# Stage 6.2 — smooth simulation-object presentation

This is a read-only, simulator-style **rendering** update based on
`fix/map-trails-update-jitter` (PR #11). No protocol revision, network commands,
MAVLink vehicle modification, or authoritative position changes are introduced.

## Design

The existing `SimulationObjectProtocol` and `SimulationObjectManager`
continue to validate and store the latest incoming state. The authoritative
`SimulationObject.coordinate` and `heading` still represent raw telemetry.
A separate `SimulationObjectPresentation` retains at most **16** recent
valid per-object frames and samples the displayed pose.

- **One shared 16 ms Qt timer** in `SimulationObjectManager` targets
  approximately 60 frames/s. It starts when a missile state is received
  and stops when there are no active missiles; no per-object animation
  timers are created. This is a **target**, not a measured frame rate.
- Each frame stores the **validated sim_time_ns**, monotonic receive time,
  coordinate, altitude datum, heading and (when present) NED velocity.
  Presentation is delayed **100 ms** behind receive time to interpolate
  between recent frames. Received simulation-time differences are mapped to
  elapsed wall time by the frame pair.
- Position follows a geographic great-circle interpolation with altitude
  interpolated separately. Heading uses the shortest angular path, including
  wraparound at 0°/360°.
- When no newer frame is available, extrapolation lasts at most **200 ms of
  presentation wall time**, based on the existing simulator-supplied NED
  velocity. Its simulation speed comes from the last two frames, capped
  at **4x**; the result then freezes. No guidance/trajectory calculation
  occurs.
- Invalid position, changes of altitude datum, timestamp rewind/duplicate,
  receive gaps beyond 500 ms or simulation gaps beyond 2 seconds discard
  the buffered interpolation segment rather than crossing discontinuities.
  Stale/completed/disconnected objects stop animating; new sessions have
  newly constructed objects/buffers.

## UI synchronization

- `renderCoordinate`, `renderHeading`, `renderHeadingValid`, and
  `renderSimTimeNs` are **presentation-only** read-only properties on
  `SimulationObject`, signaled separately from authoritative `changed`.
- QtLocation and GeoMap 2D/3D missile markers use the same render pose as
  their read-only LOS geometry. The authoritative coordinate remains
  available separately.
- The existing `MapTrailHistory` still holds real raw samples with
  its monotonic wall-clock lifetime and stable geographic anchor.
  Simulation samples additionally preserve the exact producer
  `sim_time_ns` as an integer, without precision loss through QML
  floating-point conversion.
- The drawn simulation trail includes only history samples at or before
  `renderSimTimeNs`, plus **one transient visual tip** to the rendered
  missile position. That tip is never appended to `MapTrailHistory`:
  expiration, distance accounting, and history fidelity are unchanged.
- QtLocation's GPU trail mesh refreshes its presentation tip at most
  every 33 ms. GeoMap's GPU ribbon is similarly coalesced to 33 ms.
  Vehicle trails and MAVLink aircraft remain on their previous path.

The original reported-range field and black badge still display only the
**simulator-reported value**. The numeric missile label, LOS target binding,
and all independent trail settings remain unchanged.

## Windows acceptance plan

1. Close QGroundControl before building (avoids Windows `LNK1168`).
2. Fetch and switch to `feat/simobjects-stage6-smooth-animation`,
   then build with VS Code **Ctrl+Shift+B**.
3. Run:

   ```powershell
   ctest --preset Windows-debug -R "(SimulationObjectTest|MapTrailHistoryTest|FlightPathGeometryTest|TrajectoryPointsTest)" --output-on-failure
   .\run-qgc.ps1
   ```

4. With **real kernel-sim telemetry** and MAVLink NPC aircraft:
   inspect moving missiles at low packet rates (~5–15 Hz) and ordinary
   rates (~30–60 Hz). Expect smoother movement than the previous
   raw-packet display. Test turning through 359° → 1°.
5. In both QtLocation and GeoMap (2D/3D), verify that LOS is attached to
   the same displayed missile position while the target endpoint follows
   the real MAVLink vehicle. Verify the trail tip reaches the missile
   without extending ahead of it.
6. Stop sending packets, test a brief network gap, then resume.
   The presentation must stop extrapolating after 200 ms and must never
   jump along an old buffer when the stream resumes after a large gap.
7. Test `position: null`, paused/stationary states, completion and
   session restart: invalid positions must hide LOS/marker, and
   retired sessions must not leak old display states.
8. Test 10 simultaneous missiles, rapid map drag/zoom, and all trail
   styles to detect frame pacing regressions. Check trail expiration
   at 10 seconds when telemetry stops. Normal MAVLink vehicles should
   be unaffected.

### Current limitations

- **No Windows compilation, QML engine smoke test, CTest or runtime FPS
  measurements were possible from GitHub access.** Keep the PR draft until
  these checks are complete.
- A 100 ms presentation buffer adds intentional visual latency; it
  does not alter authoritative telemetry or reported range.
- A 16-frame buffer is intended for normal simulation update rates,
  not arbitrary hundreds of packets per second. Visual mesh updates
  remain bounded but may still be CPU-intensive at high object counts.

