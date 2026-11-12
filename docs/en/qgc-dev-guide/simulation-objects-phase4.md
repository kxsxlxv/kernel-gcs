# Simulation objects — Phase 4: settings UI

Phase 4 makes the existing simulation-only UDP receiver preferences available
through the application's generated settings pages. It **does not** change the
UDP protocol, missile rendering, or normal MAVLink vehicle handling.

## UI

Open **Application Settings → Simulation Objects** (radar icon). The page uses
the QGC Fact settings model, so edits persist in the application's existing
QSettings INI file — for the current developer build, **QGroundControl Daily.ini**
— and do not require editing a text file manually.

- **Simulation Receiver**: enable/disable, listen address and port, expected
  sender IP, and expected producer ID.
- **Display and Notifications**: suppress noncritical vehicle-connected
  notifications while simulation mode is enabled.
- **Track Retention**: stale, removal, and completed-object retention
  timeouts in milliseconds.

The receiver reacts to endpoint/source changes by restarting its socket;
the resulting brief loss of tracked objects is expected. Timeout setting
changes affect lifecycle behavior without a socket restart. The settings
metadata provides the usual validation ranges.

**Security**: `allowedSender` is an IP filter and `producerId` is a
self-declared identifier; neither authenticates a remote producer. Keep the
loopback defaults unless the simulator is on a trusted machine/network.
Never use an unrestricted listener on an untrusted network.

## Windows 10 acceptance

1. Switch to `feat/simobjects-phase4`, then **Ctrl+Shift+B** to regenerate
   the QML settings page and build, and launch QGC with `run-qgc.ps1`.
2. Open Application Settings and verify **Simulation Objects** is present.
   Confirm the fields reflect the receiver values in `QGroundControl Daily.ini`.
3. Toggle the receiver **off**, confirm the SIM overlay disappears; toggle
   it **on**, confirm the receiver returns to **Listening** when bound.
4. Start the missile-only fixture (two real MAVLink NPCs remain untouched):

   ```powershell
   python .\tools\simobjects\phase1_publisher.py --duration 120 --rate 30 --center-lat 43.41 --center-lon 39.94
   ```

   Confirm `SIM Receiving` and visible missile silhouettes. Stop the
   fixture and verify **Stale** then eventual track expiration. Toggle
   the port to an unused value and back to validate live socket rebind.
5. Run `ctest --preset Windows-debug -R SimulationObjectTest --output-on-failure`.
   The earlier Windows DLL-path CTest fix is inherited from Phase 2.

### Out of scope

This settings page exposes only passive simulation telemetry reception and
lifecycle configuration. No simulated weapon control, target binding,
line-of-sight, range-to-target, or engagement features are introduced.
