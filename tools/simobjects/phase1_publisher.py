#!/usr/bin/env python3
"""Deterministic Phase 1 simulation-object fixture; sends only telemetry, never commands."""
import argparse
import json
import math
import socket
import struct
import time
import uuid

STEP_NS = 2_500_000
LAT, LON = 56.95, 24.11


def identity(seed, label):
    return str(uuid.uuid5(uuid.NAMESPACE_DNS, f"kernel-gcs:{seed}:{label}"))


def location(north, east, altitude):
    return {
        "frame": "WGS84_GEODETIC",
        "lat_deg": LAT + north / 111_320.0,
        "lon_deg": LON + east / (111_320.0 * math.cos(math.radians(LAT))),
        "altitude_m": altitude,
        "altitude_datum": "AMSL_EGM96",
    }


def velocity(t):
    if t < 3:
        return (95, 0, 0)
    if t < 6:
        return (0, 95, 0)
    if t < 9:
        return (-95, 0, 0)
    if t < 12:
        return (0, -95, 0)
    return (95, -1.65 if t < 13.5 else 1.65, 0)


def envelope(seed, t, epoch, message):
    local = t if epoch == 1 else t - 30
    tick = round(local * 400)
    return {
        "schema": "kernel.qgc.simobjects", "major": 1, "minor": 0,
        "message": message,
        "session_id": identity(seed, f"session-{epoch}"), "epoch": str(epoch),
        "source": {"producer_id": "qgc-fixture",
                   "instance_id": identity(seed, "instance-2" if 28 <= t < 30 else "instance-1")},
        "tick": str(tick), "sim_time_ns": str(tick * STEP_NS),
    }


def object_state(seed, t, epoch, name, seq):
    local = t if epoch == 1 else t - 30
    missile = name.startswith("missile")
    packet = envelope(seed, t, epoch, "state")
    vn, ve, vd = velocity(local)
    if missile:
        number = 2 if name.endswith("2") else 1
        pos = location(-1200 + vn * local * (1 if number == 1 else 0.6),
                       -500 + ve * local * (1 if number == 1 else -0.6), 360 + number * 30)
        motion = "moving"
        target_id = 12 if number == 1 and 15 <= t < 30 else 11
        packet["target"] = {"npc_id": f"npc-{target_id}", "generation": "1", "valid": True,
                            "mavlink": {"source_key": "qgc-fixture-npc", "sysid": target_id, "compid": 1}}
        packet["engagement"] = {"slant_range_m": None, "range_valid": False, "range_sim_time_ns": None}
    else:
        number = 12 if name.endswith("12") else 11
        paused = number == 11 and 5 <= t < 10
        progress = min(local, 5) if paused else (local - 5 if local >= 10 else local)
        pos = location(1800 if number == 11 else 1200 + local * 20,
                       300 + progress * 40 if number == 11 else -800, 1000 if number == 11 else 900)
        vn, ve, vd = (0, 0, 0) if paused else ((0, 40, 0) if number == 11 else (20, 0, 0))
        motion = "paused" if paused else "moving"
        packet["mavlink_binding"] = {"source_key": "qgc-fixture-npc", "sysid": number, "compid": 1}
    packet.update({
        "seq": seq, "object": {"id": name, "generation": "1", "kind": "missile" if missile else "npc",
                              "lifecycle": "active", "motion_state": motion},
        "position": pos,
        "velocity": {"frame": "NED", "north_mps": vn, "east_mps": ve, "down_mps": vd},
    })
    return packet


def packets(seed, index, rate, sequences):
    t = index / rate
    epoch = 2 if t >= 30 else 1
    local = t if epoch == 1 else t - 30
    output = []
    if index % rate == 0:
        announce = envelope(seed, t, epoch, "session_announce")
        announce.update({"session_seq": round(local), "paused": False,
                         "simulation_speed": 2.0 if 16 <= t < 19 else 1.0,
                         "fixed_step_ns": str(STEP_NS)})
        output.append(announce)
    if 25 <= t < 28:
        return [], []
    names = ["npc-11", "npc-12", "missile-1"]
    if 10 <= t < 30:
        names.append("missile-2")
    if 22.5 <= t < 30:
        names.remove("missile-1")
    for name in names:
        if name == "missile-1" and 20 <= t < 20.65:
            continue
        key = (epoch, name)
        seq = sequences.get(key, 0)
        sequences[key] = seq + 1
        data = object_state(seed, t, epoch, name, seq)
        if name == "missile-2" and abs(t - 21) < 0.5 / rate:
            data["position"] = None
        if name == "missile-2" and abs(t - 22) < 0.5 / rate:
            data["position"]["lat_deg"] = 91.0
        output.append(data)
    if abs(t - 22.5) < 0.5 / rate:
        completion = envelope(seed, t, epoch, "object_end")
        completion.update({"seq": sequences[(epoch, "missile-1")],
                           "object": {"id": "missile-1", "generation": "1", "kind": "missile"},
                           "reason": "fixture_completed"})
        output.append(completion)
    if abs(t - 21.5) < 0.5 / rate:
        output.append(object_state(seed, 19.0, epoch, "missile-1", 2))
    # QGC's ordinary Vehicle path sees only NPCs. No MAVLink missile heartbeats.
    npcs = [object_state(seed, t, epoch, name, 0) for name in ("npc-11", "npc-12")]
    return output, npcs


def crc_x25(data, extra):
    crc = 0xFFFF
    for byte in data + bytes([extra]):
        x = byte ^ (crc & 0xFF)
        x ^= (x << 4) & 0xFF
        crc = ((crc >> 8) ^ (x << 8) ^ (x << 3) ^ (x >> 4)) & 0xFFFF
    return crc


class Mavlink:
    def __init__(self):
        self.sequences = {}

    def frame(self, system, msgid, payload, extra):
        seq = self.sequences.get(system, 0)
        self.sequences[system] = (seq + 1) & 255
        header = bytes([len(payload), 0, 0, seq, system, 1]) + msgid.to_bytes(3, "little")
        return b"\xfd" + header + payload + struct.pack("<H", crc_x25(header + payload, extra))

    def heartbeat(self, system):
        return self.frame(system, 0, struct.pack("<IBBBBB", 0, 1, 0, 0, 4, 3), 50)

    def position(self, system, packet):
        p = packet["position"]
        v = packet["velocity"]
        payload = struct.pack("<IiiiihhhH", 0,
                              round(p["lat_deg"] * 1e7), round(p["lon_deg"] * 1e7),
                              round(p["altitude_m"] * 1000), 0,
                              round(v["north_mps"] * 100), round(v["east_mps"] * 100),
                              round(v["down_mps"] * 100), 0)
        return self.frame(system, 33, payload, 104)


def run(args):
    sequences = {}
    mav = Mavlink()
    counts = {"sim_packets": 0, "mavlink_packets": 0}
    overlay = (args.overlay_host, args.overlay_port)
    aircraft = (args.mavlink_host, args.mavlink_port)
    socket_out = None if args.dry_run else socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    start = time.monotonic()
    try:
        for index in range(math.ceil(args.duration * args.rate)):
            t = index / args.rate
            if socket_out:
                time.sleep(max(0, start + t / args.speed - time.monotonic()))
            updates, npcs = packets(args.seed, index, args.rate, sequences)
            for update in updates:
                payload = json.dumps(update, separators=(",", ":"), allow_nan=False).encode()
                if len(payload) > 1200:
                    raise ValueError(f"packet exceeds 1200 bytes: {len(payload)}")
                if socket_out:
                    socket_out.sendto(payload, overlay)
                counts["sim_packets"] += 1
            if index % max(1, args.rate // 10) == 0:
                for npc in npcs:
                    system = 11 if npc["object"]["id"] == "npc-11" else 12
                    payloads = [mav.position(system, npc)]
                    if index % args.rate == 0:
                        payloads.append(mav.heartbeat(system))
                    for payload in payloads:
                        if socket_out:
                            socket_out.sendto(payload, aircraft)
                        counts["mavlink_packets"] += 1
    finally:
        if socket_out:
            socket_out.close()
    return counts


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--overlay-host", default="127.0.0.1")
    cli.add_argument("--overlay-port", type=int, default=14660)
    cli.add_argument("--mavlink-host", default="127.0.0.1")
    cli.add_argument("--mavlink-port", type=int, default=14550)
    cli.add_argument("--rate", type=int, default=30)
    cli.add_argument("--duration", type=float, default=33.0)
    cli.add_argument("--speed", type=float, default=1.0)
    cli.add_argument("--seed", type=int, default=42)
    cli.add_argument("--dry-run", action="store_true")
    cli.add_argument("--self-test", action="store_true")
    args = cli.parse_args()
    if min(args.rate, args.duration, args.speed) <= 0:
        cli.error("rate, duration and speed must be positive")
    if args.self_test:
        sequences = {}
        assert packets(42, 0, 30, sequences)[0][0]["message"] == "session_announce"
        assert any(p.get("object", {}).get("motion_state") == "paused"
                   for p in packets(42, 7 * 30, 30, sequences)[0])
        assert any(p.get("target", {}).get("npc_id") == "npc-12"
                   for p in packets(42, 16 * 30, 30, sequences)[0])
        assert any(p["message"] == "object_end" for p in packets(42, 675, 30, sequences)[0])
        assert not packets(42, 26 * 30, 30, sequences)[0]
        assert packets(42, 30 * 30, 30, sequences)[0][0]["epoch"] == "2"
        args.dry_run = True
    print(("SELF-TEST" if args.self_test else "DRY-RUN" if args.dry_run else "SENT"), run(args))


if __name__ == "__main__":
    main()
