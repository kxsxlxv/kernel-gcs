# Phase 5 — Configurable map trails (draft)

This stacked branch builds on `feat/simobjects-phase4`. It is an independent
**visualization-only** feature for both ordinary MAVLink `Vehicle` objects and
read-only `SimulationObject` tracks.

## Application Settings → Map Trails

Two independent groups, **Vehicle Trails** and **Simulation Object Trails**,
share the same configuration schema but persist to distinct existing Fact
settings groups: `FlightMap/vehicleTrail*` and `SimulationObject/trail*`.

| Setting | Vehicle default | Simulation default |
| --- | --- | --- |
| Enabled | Yes | Yes |
| Lifetime | 10 s | 10 s |
| Style | Solid | Fading |
| Color | Red | Orange |
| Width | 3 px | 3 px |
| Opacity | 1.0 | 1.0 |
| Clear when stationary | No | No |

Lifetime choices are **Unlimited / 1 / 3 / 5 / 10 / 15 / 30 / 60 seconds**.
Styles are **Solid / Dashed / Dotted / Fading / None**. Color selection uses
nine named color presets; alpha and width Facts are bounded to `0..1` and
`1..12` screen pixels, respectively. None disables history accumulation.
The optional **Clear trail when stationary** preference clears immediately on
reported stationary/paused Simulation Object motion or on a Vehicle
`groundSpeed.rawValue` below 0.5 m/s. If clear is off, the trail simply
stops growing while stationary and ages out naturally.

Facts persist through QGC's existing `QSettings` mechanism. Changes apply
live, without application restart. Receiver settings, MAVLink vehicle state,
simulation-object identity and existing connection code remain untouched.

## Architecture

- `MapTrailHistory` is the one shared C++ timestamped-history type. It
  samples real movement at most once per 100 ms, ignores moves below 0.75 m
  in 3D distance, and retains a maximum of **2,048** samples per object
  even when Unlimited is selected.
- A dedicated 200 ms monotonic-clock timer expires finite histories even with
  zero incoming position updates. Old samples **never** have their timestamps
  refreshed. A stopped object loses its line while keeping its marker.
- QML delegates own one history per object. Destruction of a Vehicle or
  SimulationObject delegate releases its history, timer, and geometry. Selecting
  a different active Vehicle does not reassign any history.
- This history is **separate from `Vehicle::trajectoryPoints`**. The latter
  retains its existing point/direction simplification and flight-distance
  accounting; no distance logic is modified.
- QtLocation uses one `MapTrailCanvas` per tracked object, projecting its
  timestamped positions into screen coordinates at each map camera change.
  Dashes/dots are drawn with path dash patterns, not by deleting samples.
  Fade uses a linear alpha gradient along each segment.
- GeoMap uses the existing `GeoMapFlightPath` / `FlightPathGeometry`
  ribbon and shader with two additional per-vertex attributes (distance
  and fade). Solid/dash/dot/fade is evaluated in the fragment shader,
  width in screen pixels by the vertex shader. Append-only histories update
  the GPU geometry incrementally for non-fading styles. Pruning rebuilds
  and replaces vertex data, so expired geometry cannot survive on screen.
  Fading refreshes the vertex fades every 200 ms.

Altitude: ordinary Vehicles use GeoMap's existing DEM/home AMSL correction.
SimulationObjects with `AMSL_EGM96` share the active Vehicle's display bias;
objects with `WGS84_ELLIPSOID` or `AMSL_UNSPECIFIED` are rendered at
sampled terrain height, rather than misrepresenting ellipsoidal height as
AMSL. GeoMap's existing depth-pull and terrain scale are preserved.

## Build and acceptance test (Windows 10)

1. `git fetch origin`, switch to `feat/map-trails-phase5`, and build
   with the existing **Ctrl+Shift+B** task; the CMake-generated Fact
   metadata, QML module and settings page need regeneration.
2. Launch using `run-qgc.ps1`. Confirm **Application Settings → Map Trails**
   shows independent defaults and all lifetime/style/color/opacity/width
   controls.
3. Start two normal MAVLink aircraft from your current simulator and the
   existing independent missile fixture:

   ```powershell
   python .\tools\simobjects\phase1_publisher.py --duration 120 --rate 30 --center-lat 43.41 --center-lon 39.94
   ```

4. Confirm separate vehicle and missile trails. On each category select
   1, 3, 10, and Unlimited; after stopping telemetry verify expiration
   occurs with no new packets and that markers remain.
5. Toggle `Enabled` and `None`, vary color/width/opacity and all four
   visible styles, and verify the other category remains unchanged.
6. Verify the clear-when-stationary policy (Vehicle ground speed and
   SimulationObject motion_state), resumed movement, destruction of an
   object, multiple moving objects, active Vehicle switching, and both
   GeoMap 2D/3D and QtLocation 2D modes.
7. Run:

   ```powershell
   ctest --preset Windows-debug -R "(MapTrailHistoryTest|FlightPathGeometryTest|TrajectoryPointsTest|SimulationObjectTest)" --output-on-failure
   ```

## Review notes / performance limits

- Qt 6 Canvas uses an image-backed renderer, so **QtLocation's full-map
  Canvas redraw rate and QImage uploads need profiling** at high object
  counts and 4K display sizes; no performance claim is made without data.
  Sampling is limited to 10 Hz per object and memory is bounded.
- GeoMap shaders implement dash/dot spacing in approximate projected
  screen pixels; at strong 3D perspective and camera motion, dash cadence
  may vary slightly with depth.
- The maximum trail capacity is 2,048 points per object. At 10 Hz, an
  Unlimited trail represents roughly 205 seconds of continuously moving
  history; it is **not** permanent full-flight logging.
- Per-object overrides are not wired yet; both renderers read category
  Facts through one QML property layer, suitable for a later override.
- No local Windows build, Qt Quick visual test, or performance measurement
  has run through the GitHub connector. The PR must remain **draft**
  until these gates pass.
