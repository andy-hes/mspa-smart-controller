#!/usr/bin/env python3
"""Passive dual-UART sniffer for MSpa-like remote links.

Safety:
- Passive listen only.
- Does not transmit any data.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import threading
import time
from dataclasses import dataclass
from typing import Optional

try:
    import serial
except ImportError as exc:  # pragma: no cover
    raise SystemExit("pyserial is required: pip install pyserial") from exc


SYNC = 0xA5


@dataclass
class FrameRecord:
    ts_iso: str
    direction: str
    raw_hex: str
    valid_checksum: Optional[bool]
    decoded: str


def calc_checksum(sync: int, command: int, value: int) -> int:
    return (sync + command + value) & 0xFF


def decode_frame(frame: bytes) -> tuple[Optional[bool], str]:
    if len(frame) != 4:
        return None, "not_4_bytes"

    sync, command, value, checksum = frame
    if sync != SYNC:
        return None, "unknown_sync"

    expected = calc_checksum(sync, command, value)
    valid = checksum == expected

    names = {
        0x01: "heater",
        0x02: "filter",
        0x03: "bubbles",
        0x04: "target_temperature",
        0x06: "current_temperature",
        0x08: "bath_status",
        0x0B: "reset",
        0x0D: "jet",
        0x0E: "ozone",
        0x15: "uvc",
        0x16: "heartbeat",
    }
    cmd_name = names.get(command, f"unknown_cmd_0x{command:02X}")
    return valid, f"{cmd_name} value={value}"


def iter_frames(port: "serial.Serial", stop_event: threading.Event):
    buf = bytearray()
    while not stop_event.is_set():
        data = port.read(1)
        if not data:
            continue
        b = data[0]
        if not buf and b != SYNC:
            continue
        buf.append(b)
        if len(buf) == 4:
            yield bytes(buf)
            buf.clear()


def sniff_direction(
    port_name: str,
    baud: int,
    direction: str,
    stop_event: threading.Event,
    jsonl_path: str,
):
    with serial.Serial(port_name, baudrate=baud, timeout=0.2) as ser, open(
        jsonl_path, "a", encoding="utf-8"
    ) as out:
        for frame in iter_frames(ser, stop_event):
            ts = dt.datetime.now(dt.timezone.utc).astimezone().isoformat()
            valid, decoded = decode_frame(frame)
            record = FrameRecord(
                ts_iso=ts,
                direction=direction,
                raw_hex=" ".join(f"{x:02X}" for x in frame),
                valid_checksum=valid,
                decoded=decoded,
            )
            line = json.dumps(record.__dict__, ensure_ascii=True)
            out.write(line + "\n")
            out.flush()
            print(f"[{record.ts_iso}] {direction:14} {record.raw_hex} valid={valid} {decoded}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Passive MSpa UART sniffer")
    parser.add_argument("--spa-port", required=True, help="Serial port for spa->remote direction")
    parser.add_argument("--remote-port", help="Serial port for remote->spa direction")
    parser.add_argument("--baud", type=int, default=9600)
    parser.add_argument("--out", default="sniffer_log.jsonl", help="Output JSONL log file")
    args = parser.parse_args()

    stop_event = threading.Event()
    threads = []

    threads.append(
        threading.Thread(
            target=sniff_direction,
            kwargs={
                "port_name": args.spa_port,
                "baud": args.baud,
                "direction": "spa_to_remote",
                "stop_event": stop_event,
                "jsonl_path": args.out,
            },
            daemon=True,
        )
    )

    if args.remote_port:
        threads.append(
            threading.Thread(
                target=sniff_direction,
                kwargs={
                    "port_name": args.remote_port,
                    "baud": args.baud,
                    "direction": "remote_to_spa",
                    "stop_event": stop_event,
                    "jsonl_path": args.out,
                },
                daemon=True,
            )
        )

    for t in threads:
        t.start()

    print("Passive sniffing started. Press Ctrl+C to stop.")
    try:
        while True:
            time.sleep(0.5)
    except KeyboardInterrupt:
        stop_event.set()
        for t in threads:
            t.join(timeout=1.0)
        print("Stopped.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
