# Simulation objects — Phase 3 groundwork: passive receiver diagnostics

This small Phase 3 UI step adds a **read-only simulation-object status indicator** to
Fly View. It is a display/diagnostic improvement, not a target acquisition or
engagement feature.

## Behavior

- A small **SIM N objects** strip appears on Fly View when the existing
  `SimulationObject/enabled` setting is true and the map is the main view.
- `N` is the size of the accepted simulation-object model (including retained
  stale/completed entries), **not** the number of real MAVLink vehicles or
  proof that a UDP socket is bound.
- A yellow dot with **0 objects** means no objects are currently in the model;
  that can mean a new connection, expired tracks or an absent publisher.
  It does **not** claim that the UDP socket has failed.
- A green dot indicates that at least one simulation object is currently in
  the model; it is **not** an authentication or network-health indicator.
- The indicator is hidden when the receiver setting is disabled, the map is in
  picture-in-picture mode or fullscreen video is shown.
- One shared QML component is placed in `FlyView.qml` above the map-engine
  switch, so both QtLocation and GeoMap receive the same display.

All missile markers remain read-only, and ordinary MAVLink NPCs remain unchanged.

## Windows 10 verification

1. Check out `feat/simobjects-phase3`, use the existing `Ctrl+Shift+B`
   build, and start QGC with the existing `run-qgc.ps1` launcher.
2. Leave `[SimulationObject] enabled=true` in the **QGroundControl Daily.ini**
   file used by this build. Open Fly View: `SIM 0 objects` should appear before
   starting the fixture. If other producers are active, the count may already
   be greater than zero.
3. In a separate terminal, run:

   ```powershell
   python .\tools\simobjects\phase1_publisher.py --duration 120 --rate 30 --center-lat 43.41 --center-lon 39.94
   ```

4. The status count should increase as accepted objects appear, and eventually
   drop when they expire. The test fixture sends **no synthetic MAVLink NPCs**.
5. Repeat with GeoMap enabled, the map in PiP mode, and fullscreen video to
   verify the indicator's visibility.
6. Run:

   ```powershell
   ctest --preset Windows-debug -R SimulationObjectTest --output-on-failure
   ```

   Phase 2's Windows-only CTest DLL-path change requires a reconfigure/build
   before the new test environment is applied. A `0xc0000135` exit code means
   DLL loading failed before QtTest started, not that an assertion failed.

## Scope

This branch contains **no missile-to-target mapping, target range or
line-of-sight calculation, weapon guidance, engagement controls, intercept
prediction or new MAVLink vehicles**. Those do not form part of the passive
diagnostic feature.
