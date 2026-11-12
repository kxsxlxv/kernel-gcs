# Simulation objects — Phase 1 (read-only)

This is the **dedicated simulation-object receiver**, not a MAVLink missile adapter. It does not produce QGC
`Vehicle` instances, does not select active aircraft, does not send MAVLink commands, and does not
create map markers yet (map rendering is Phase 2). Aircraft and NPC MAVLink handling remains unchanged.
**Stop publishing missile HEARTBEAT messages on the ordinary MAVLink link** before using this mode:
the older publisher is intentionally unsupported and QGC will still treat its heartbeats as vehicles.

## Configure

The receiver is disabled by default. QGC persists the following Fact settings under the normal
`SimulationObject` QSettings group:

| Fact | Default | Meaning |
| --- | --- | --- |
| `enabled` | false | Enable the simulation UDP receiver |
| `suppressVehicleConnectedPopups` | true | While enabled, suppress noncritical `Connected to Vehicle` dialogs; critical warnings remain |
| `bindAddress` | `127.0.0.1` | Local numeric listen address |
| `listenPort` | `14660` | UDP listen port |
| `allowedSender` | `127.0.0.1` | **Exact** numeric source IP |
| `producerId` | `qgc-fixture` | Required `source.producer_id` |
| `completedRetentionMs` | `1000` | Terminal object visibility retention |
| `staleTimeoutMs` | `2000` | Indicate stale/disconnected |
| `removalTimeoutMs` | `5000` | Remove inactive objects without completing them |

Phase 1 does not yet add these controls to a settings page. QGC uses Qt `QSettings`, which stores
application preferences in an INI file (not the Windows registry). Close QGC and locate the exact
INI pathname from its `Settings location` log entry in `QGCApplication.cc`. Add or edit the following
section and restart QGC:

```ini
[SimulationObject]
enabled=true
suppressVehicleConnectedPopups=true
bindAddress=127.0.0.1
listenPort=14660
allowedSender=127.0.0.1
producerId=qgc-fixture
```

Back up this file first. The settings group will gain a UI in Phase 4.

Loopback is the safe default. For a separate Windows computer, configure `bindAddress` as a specific
interface address and `allowedSender` as the simulator computer's IP. Do not expose the listener on
untrusted networks: peer filtering and a self-declared producer ID are **not authentication**. Use a
VPN or authenticated tunnel for such paths.

Changing the receiver endpoint or producer setting restarts its socket and clears the current objects.
Changing the retention/stale timeouts takes effect without restarting the receiver.

## Wire and lifecycle

One UTF-8 JSON object per UDP datagram, maximum **1200 bytes**, schema `kernel.qgc.simobjects`,
version `major:1`, `minor:0`. The initial `session_announce` is mandatory before object updates.
A newer announced session/epoch replaces the old object set without inventing completion events.
Late packets from retired sessions, duplicate/out-of-order object sequences, backwards simulation time,
malformed coordinates, non-finite values and unauthorized sources are rejected.

The immutable object key is `(session_id, epoch, object.id, object.generation)`. A publisher process
restart alone is not completion. `object_end` is terminal for the same object key; a new generation or
session is necessary for reuse. Completion retention and network freshness use a monotonic receive clock,
never the simulation clock. A paused or stationary NPC is not completed. A sender disappearance produces
stale/disconnected status, not a synthetic intercept.

Objects are bounded to 256 concurrent entries and 4096 remembered per-session sequence/tombstone keys;
admission fails closed when these limits are reached. For a completed object whose initial states
were never received, the terminal key is still remembered (no phantom marker is created).

This initial implementation does not yet resolve NPC target bindings against QGC `Vehicle` positions,
draw engagement lines, interpolate flight paths, or display range. It does validate their versioned
identity metadata for later phases. Only the receiver/model/lifecycle are implemented here.

## Deterministic test

To observe Phase 1's deliberately nonvisual model, launch the existing Windows QGC executable with
`--logging=SimulationObjects.Manager --log-output`. It logs admitted sessions and object creation,
explicit completion and removal, without printing every 30 Hz position update. A regular aircraft
still uses QGC's existing `Vehicle`/MAVLink handling; Phase 2 will add map markers.

The Phase 0 standalone Python fixture `qgc_simobjects_fixture.py` works with the v1 protocol.
Place it on the Windows machine or simulator host, then run:

```powershell
python .\qgc_simobjects_fixture.py --self-test
python .\qgc_simobjects_fixture.py --dry-run --duration 33
python .\qgc_simobjects_fixture.py --duration 33 --rate 30 --overlay-host 127.0.0.1 --overlay-port 14660 --mavlink-host 127.0.0.1 --mavlink-port 14550
```

The fixture sends NPC aircraft as ordinary MAVLink HEARTBEAT/GLOBAL_POSITION_INT and missiles as
simulation UDP JSON. No flight-control commands are sent. When running across PCs, replace the
destination addresses with the Windows computer's reachable IP and configure the receiver/firewall.

### Windows 10: preserve the existing working build

The local `run-qgc.ps1` script is a **runtime launcher only**. It points at
`build\\Debug\\QGroundControl.exe` and prepends Qt 6.12.0 MSVC and GStreamer runtime paths to `PATH`.
It does not activate the MSVC compiler, provide Ninja, or set `QT_ROOT_DIR` for CMake.

Open **Developer PowerShell for VS 2022** (x64) and run from the repository root:

```powershell
$env:QT_ROOT_DIR = "C:\\Qt\\6.12.0\\msvc2022_64"
$env:PATH = "C:\\Qt\\Tools\\Ninja;$env:PATH"
Test-Path "$env:QT_ROOT_DIR\\lib\\cmake\\Qt6\\qt.toolchain.cmake"
Get-Command cl, ninja, cmake
cmake --fresh --preset Windows-debug
cmake --build --preset Windows-debug --parallel 8
ctest --preset Windows-debug -R SimulationObjectTest --output-on-failure
```

If Ninja is not under `C:\\Qt\\Tools\\Ninja`, add the directory containing your already-installed
`ninja.exe` to the shell's `PATH` instead (Visual Studio may also include one).
The missing ccache/sccache warning is optional and **not** the cause of configuration failure.
The `--fresh` flag resets only the separate `build\\Windows-debug` CMake cache, not the working
`build\\Debug` executable or its cache.

The Phase 1 binary will normally be `build\\Windows-debug\\Debug\\QGroundControl.exe`, and
its generated QML module path `build\\Windows-debug\\qml`. To keep the original launcher
unchanged, copy it to `run-qgc-phase1.ps1` and change those two paths in that copy.
This preserves the verified `run-qgc.ps1`/default-build workflow.

Do not run CTest until configure **and** compile succeed: a failed configure naturally
produces `No tests were found`. Phase 1 does not upgrade the Qt/compiler/dependencies.

Disable/revert: set `SimulationObject/enabled=false` (or undo the Phase 1 commit/PR).
The ordinary MAVLink Vehicle path is unchanged except for the separately configurable, simulation-mode-only informational connection dialog suppression.
