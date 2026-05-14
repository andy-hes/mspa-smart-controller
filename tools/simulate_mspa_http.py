#!/usr/bin/env python3
"""Local MSpa HTTP API simulator for testing the Homey app without ESP32.

Implements the same endpoints as firmware/homey-http/src/main.cpp.
"""

from __future__ import annotations

import argparse
import json
import threading
import time
from dataclasses import dataclass
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse


@dataclass
class SpaState:
    filter_on: bool = True
    heater_on: bool = True
    auto_restore_enabled: bool = True
    bubbles_level: int = 0
    target_temp: int = 38
    current_temp_c: float = 36.0
    bath_status: int = 3
    online: bool = True
    boot_ts: float = time.time()


class Simulator:
    def __init__(self, heat_rate: float, cool_rate: float):
        self.heat_rate = heat_rate
        self.cool_rate = cool_rate
        self.state = SpaState()
        self.lock = threading.Lock()
        self.stop_event = threading.Event()

    def ensure_safe_heater_state(self) -> None:
        if not self.state.filter_on:
            self.state.heater_on = False

    def handle_restore(self) -> None:
        if not self.state.auto_restore_enabled:
            return
        if not self.state.online:
            return
        self.state.filter_on = True
        self.ensure_safe_heater_state()
        self.state.heater_on = True

    def to_status(self) -> dict:
        with self.lock:
            return {
                "ok": True,
                "online": self.state.online,
                "current_temperature_c": round(self.state.current_temp_c, 1),
                "target_temperature_c": self.state.target_temp,
                "filter_on": self.state.filter_on,
                "heater_on": self.state.heater_on,
                "bubbles_level": self.state.bubbles_level,
                "auto_restore_enabled": self.state.auto_restore_enabled,
                "bath_status": self.state.bath_status,
                "uptime_s": int(time.time() - self.state.boot_ts),
            }

    def simulate_tick(self) -> None:
        with self.lock:
            if not self.state.online:
                return

            if self.state.filter_on and self.state.heater_on and self.state.current_temp_c < self.state.target_temp + 0.2:
                self.state.current_temp_c += self.heat_rate
            else:
                self.state.current_temp_c -= self.cool_rate

            self.state.current_temp_c = max(10.0, min(45.0, self.state.current_temp_c))

            if self.state.online:
                self.state.bath_status = 3
            else:
                self.state.bath_status = 0


class Handler(BaseHTTPRequestHandler):
    simulator: Simulator

    def _json(self, code: int, payload: dict) -> None:
        data = json.dumps(payload).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        if parsed.path == "/api/status":
            self._json(HTTPStatus.OK, self.simulator.to_status())
            return

        self._json(HTTPStatus.NOT_FOUND, {"ok": False, "error": "not found"})

    def do_POST(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        path = parsed.path

        with self.simulator.lock:
            st = self.simulator.state

            if path == "/api/filter/on":
                st.filter_on = True
            elif path == "/api/filter/off":
                st.filter_on = False
                self.simulator.ensure_safe_heater_state()
            elif path == "/api/heater/on":
                st.filter_on = True
                st.heater_on = True
            elif path == "/api/heater/off":
                st.heater_on = False
            elif path == "/api/bubbles/on":
                st.bubbles_level = 1
            elif path == "/api/bubbles/off":
                st.bubbles_level = 0
            elif path == "/api/auto-restore/on":
                st.auto_restore_enabled = True
            elif path == "/api/auto-restore/off":
                st.auto_restore_enabled = False
            elif path == "/api/restore":
                self.simulator.handle_restore()
            elif path == "/api/target-temperature":
                length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(length).decode("utf-8") if length > 0 else ""
                params = parse_qs(body)
                if "value" not in params:
                    self._json(HTTPStatus.BAD_REQUEST, {"ok": False, "error": "missing value"})
                    return
                try:
                    v = int(params["value"][0])
                except ValueError:
                    self._json(HTTPStatus.BAD_REQUEST, {"ok": False, "error": "invalid value"})
                    return
                if v < 20 or v > 40:
                    self._json(HTTPStatus.BAD_REQUEST, {"ok": False, "error": "value out of range"})
                    return
                st.target_temp = v

            # Simulation helper endpoints.
            elif path == "/api/sim/offline":
                st.online = False
                st.bath_status = 0
            elif path == "/api/sim/online":
                st.online = True
                st.bath_status = 3
            elif path == "/api/sim/temp":
                q = parse_qs(parsed.query)
                if "value" not in q:
                    self._json(HTTPStatus.BAD_REQUEST, {"ok": False, "error": "missing value query"})
                    return
                try:
                    t = float(q["value"][0])
                except ValueError:
                    self._json(HTTPStatus.BAD_REQUEST, {"ok": False, "error": "invalid value query"})
                    return
                st.current_temp_c = max(10.0, min(45.0, t))
            else:
                self._json(HTTPStatus.NOT_FOUND, {"ok": False, "error": "not found"})
                return

        self._json(HTTPStatus.OK, self.simulator.to_status())

    def log_message(self, fmt: str, *args) -> None:
        return


def run_background(sim: Simulator) -> None:
    while not sim.stop_event.is_set():
        time.sleep(1.0)
        sim.simulate_tick()


def main() -> int:
    parser = argparse.ArgumentParser(description="MSpa HTTP simulator for Homey app testing")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--heat-rate", type=float, default=0.08, help="Degrees C/s when heating")
    parser.add_argument("--cool-rate", type=float, default=0.02, help="Degrees C/s when cooling")
    args = parser.parse_args()

    sim = Simulator(heat_rate=args.heat_rate, cool_rate=args.cool_rate)
    Handler.simulator = sim

    bg = threading.Thread(target=run_background, args=(sim,), daemon=True)
    bg.start()

    server = ThreadingHTTPServer((args.host, args.port), Handler)

    print(f"MSpa simulator running on http://{args.host}:{args.port}")
    print("Auth disabled")
    print("Homey device host should be: <PC-IP>:<port>")
    print("Simulation helpers:")
    print("  POST /api/sim/offline")
    print("  POST /api/sim/online")
    print("  POST /api/sim/temp?value=37.5")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        sim.stop_event.set()
        server.server_close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
