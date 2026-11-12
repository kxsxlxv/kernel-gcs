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

### Use existing simulator NPCs (no synthetic MAVLink aircraft)

**Enable the separate overlay receiver first.** It defaults to disabled. Close QGC,
back up its INI preferences, then add/edit the following section in QGC's actual
settings file (look for the `Settings location` log message with application
logging enabled; see [Phase 1](simulation-objects-phase1.md) for configuration details):

```ini
[SimulationObject]
enabled=true
bindAddress=127.0.0.1
listenPort=14660
allowedSender=127.0.0.1
producerId=qgc-fixture
```

Restart QGC and start the **two real moving NPCs** through your normal MAVLink
simulator. The QGC fixture now sends **missile-only JSON by default**: it creates
no fake MAVLink NPCs and no extra simulation `npc` objects. Run it only after
QGC is open, in another terminal:

```powershell
python .\tools\simobjects\phase1_publisher.py --duration 60 --rate 30
```

The default fixture launch point is near **Riga, Latvia**
(latitude `56.95`, longitude `24.11`). If your real NPCs are flying elsewhere,
provide **their approximate map-area coordinates** so test missile markers
appear on the same map:

```powershell
python .\tools\simobjects\phase1_publisher.py --duration 60 --rate 30 --center-lat 56.95 --center-lon 24.11
```

Replace those two Riga numbers with your real simulator area. The missile paths
are scripted test motion near that center, **not** launches from, guidance
towards or tracking of the actual two NPCs. Only a real simulator's missile
producer can send authoritative missile trajectories.

If you explicitly need the old full synthetic test, add `--with-fixture-npcs`.
That opt-in sends `npc-11`/`npc-12` over MAVLink port `14550` and adds them
to the overlay; **do not use it with your real moving NPCs**.

### Troubleshooting invisible missile markers

1. Run QGC with `--logging=SimulationObjects.Manager --log-output` to see
   receiver and object-admission diagnostics. A working receiver logs
   `Simulation receiver listening` on the configured endpoint and
   `Simulation object created` when packets arrive.
2. The fixture sends from `127.0.0.1` with producer ID `qgc-fixture` to
   port `14660` unless overridden. The configured allowed sender, producer
   ID, and listener must match the actual source. For another PC, use
   appropriately configured **trusted** interface/peer addresses.
3. Zoom Fly View near the fixture center **while the publisher is running**.
   The default duration is finite; after stopping, the Phase 1 expiration
   timer removes its markers. If the log shows object creation but no marker,
   check for QML runtime warnings and verify the current Phase 2 binary is launched.
4. With `ctest --preset Windows-debug -R SimulationObjectTest --output-on-failure`
   confirm the underlying data model still passes. This alone does not verify
   the QML overlay visually.

Verify one or more labeled missile markers move separately from the normal
MAVLink aircraft, without additional vehicle dialogs. Verify completion,
stale/disconnected display, session reset, and both Fly View map engines
(`useGeoMapEngine` requires a restart).

The receiver is opt-in; disabled/default configuration continues to show no
simulation markers. On separate machines configure the allowed sender, listen
interface and publisher destination according to the Phase 1 contract.

## Scope boundary

Missile heading is not inferred in Phase 2; the diamond marker deliberately
does not imply a vehicle orientation. Target binding, LOS/range, engagement panels
and settings UI belong to subsequent phases. The normal `run-qgc.ps1` is not
modified by this work.
