# Simulation objects — Phase 3: passive receiver diagnostics

This Phase 3 increment adds a **read-only status indicator** to Fly View.
It does not connect simulated missiles to MAVLink targets or implement targeting,
engagement, guidance or intercept functions.

## Receiver states

The indicator reads the existing simulation manager, not the MAVLink vehicle
manager. It appears when `SimulationObject/enabled` is true and Fly View is
the main map.

| Display | Meaning |
| --- | --- |
| Grey SIM Listening | UDP socket successfully bound, but no active accepted session |
| Green SIM Receiving | A valid session is active and recent accepted packets have arrived |
| Yellow SIM Stale | No accepted packets within the configured stale timeout |
| Red SIM Socket error | Invalid endpoint/peer/port configuration or UDP bind failure |

The model also exposes `disabled`, but the indicator is hidden by the existing
settings-based visibility rule when the receiver is disabled.

The appended `N objects` count is the number of simulation-only entries that
are currently held, including retained stale/completed objects. **A nonzero
count does not prove ongoing reception.** The indicator checks receiver state
independently to avoid incorrectly treating retained objects as healthy
network traffic. Stale status uses monotonic receive time; duplicate,
unauthorized and malformed packets cannot refresh it.

QtLocation and GeoMap use the same indicator in `FlyView.qml`. It is hidden
when the map is in picture-in-picture mode or video is full-screen. Ordinary
MAVLink aircraft/NPCs remain entirely separate.

## Windows validation

1. Switch to `feat/simobjects-phase3` and press **Ctrl+Shift+B**. This
   rebuilds and regenerates CTest configuration.
2. Keep the receiver enabled in the build's **QGroundControl Daily.ini**, with
   `listenPort=14660`, trusted source `127.0.0.1` and `producerId=qgc-fixture`.
3. Start QGC through the existing `run-qgc.ps1` script. The SIM indicator
   should initially report **Listening**, not **Receiving**.
4. Publish telemetry:

   ```powershell
   python .\tools\simobjects\phase1_publisher.py --duration 120 --rate 30 --center-lat 43.41 --center-lon 39.94
   ```

   The indicator should turn **Receiving** while packets arrive, and
   **Stale** when they stop for at least the configured stale timeout.
   Accepted object count may return to zero afterward.
5. Test a disabled receiver, mismatched source and blocked UDP port. A
   failed bind or invalid endpoint should show **Socket error** when the
   receiver setting is enabled. A disabled receiver hides the indicator.
6. Run `ctest --preset Windows-debug -R SimulationObjectTest --output-on-failure`.
   The regression suite covers listening, receiving, stale, error and disabled
   state transitions with a temporary loopback port, without requiring an
   external simulator.

This branch contains no new launch scripts, fictional MAVLink NPCs or changes
to the working missile markers.
