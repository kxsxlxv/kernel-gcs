# Simulation objects — Phase 2 (Fly View map markers)

Phase 2 adds **read-only missile markers** to both Fly View map engines (QtLocation
and GeoMap 2D/3D), on top of the existing Phase 1 dedicated simulation UDP model.
No MAVLink Vehicle objects, fake rocket heartbeats, guided controls, or target
engagement geometry are added.

## Display behavior

- **Missiles** (`object.kind = "missile"`) with `positionValid = true` appear as orange
  diamond markers marked **M**, with the simulation object ID alongside the marker.
  `object.kind = "npc"` is **not** drawn a second time; NPC aircraft remain in QGC's
  existing MAVLink vehicle layer.
- State updates move the existing marker without creating another QML item. A
  new `generation` or `session_id` is a distinct model identity.
- Missing/null positions hide markers, rather than showing retained coordinates
  as current. Stale and disconnected markers remain at last-known valid positions
  but turn grey with status in their labels. Completed markers dim and are removed
  according to Phase 1 `completedRetentionMs`.
- GeoMap uses the reported altitude plus the same active-vehicle DEM/home correction
  as existing aircraft markers **only** for `AMSL_EGM96`; `WGS84_ELLIPSOID` and
  `AMSL_UNSPECIFIED` heights are ground-clamped until vertical datum conversion is
  implemented. QtLocation shows the reported horizontal coordinate.
- Markers are informational only: no selecting, commanding, follow tracking, automatic
  recentering, trails, LOS lines, or range figures are introduced in Phase 2.

## Verify on the existing Windows 10 setup

Keep the previously validated **VS Code `Ctrl+Shift+B` build task** and the single
`run-qgc.ps1` launcher. Switch to `feat/simobjects-phase2` and build with the same
environment; no new scripts, presets, or dependency versions are required.

```powershell
git fetch origin
git switch --track origin/feat/simobjects-phase2
# Or if the branch already exists locally: git switch feat/simobjects-phase2

# Build using your existing Ctrl+Shift+B workflow; then:
ctest --preset Windows-debug -R SimulationObjectTest --output-on-failure
python .\tools\simobjects\phase1_publisher.py --self-test
```

Enable `SimulationObject/enabled=true` in the normal QGC INI settings profile, as
described in [Phase 1](simulation-objects-phase1.md). Start QGC with `run-qgc.ps1`
and run the publisher in another terminal:

```powershell
python .\tools\simobjects\phase1_publisher.py --duration 33 --rate 30
```

Zoom the Fly View map near the fixture's coordinates; verify one labeled missile
marker updates, does not become a `Vehicle`, and never creates a vehicle dialog.
Confirm multiple ordinary MAVLink vehicles still connect normally. Verify markers
vanish on null position, expire on stale/disconnected and completion, and clear
on session reset. Repeat once with `useGeoMapEngine` enabled (requires QGC restart)
to exercise GeoMap.

The receiver is opt-in; disabled/default configuration continues to show no
simulation markers. On separate machines configure the allowed sender, listen
interface and publisher destination according to the Phase 1 contract.

## Scope boundary

Missile heading is not inferred in Phase 2; the diamond marker deliberately
does not imply a vehicle orientation. Target binding, LOS/range, engagement panels
and settings UI belong to subsequent phases. The normal `run-qgc.ps1` is not
modified by this work.
