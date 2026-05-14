# AGENTS.md

## Project: MSpa Smart Controller

This repository develops an ESP32-based controller/sniffer for MSpa inflatable hot tubs with a simple wired remote control. The target MSpa type is similar to MSpa Muse / Mist. The user's specific Mist remote does **not** have UVC or ozone buttons, so UVC/ozone features must be optional and disabled by default.

The main goal is to solve the power-loss problem: after mains power is restored, the spa does not automatically resume heating/filtering. The controller should safely restore a configured operating state after reboot/power restore.

## Important safety rules

- This project interfaces with a spa/bath product used around water. Treat this as safety-critical.
- Never suggest or implement modifications on mains-voltage circuits unless explicitly documented and isolated.
- The preferred integration point is the low-voltage wired remote cable only.
- Start with passive sniffing before enabling command injection.
- Assume ESP32 GPIO is **not 5 V tolerant**.
- Always require level shifting, opto-isolation, or verified 3.3 V UART before connecting ESP32 GPIO to the MSpa remote lines.
- Keep original product safety features intact.
- Do not bypass thermal protection, flow protection, GFCI/RCD protection, pump protection, or error states.
- On boot, never blindly enable heater before confirming spa status if status decoding is available.
- Heater should normally only be enabled together with filter/pump when the spa accepts that mode.
- Provide clear warnings in documentation before hardware connection steps.

## Known/expected hardware/protocol background

Based on similar MSpa projects, many MSpa wired remotes use a 4-wire cable:

- GND
- VCC, often 5 V
- TX from spa control box to remote
- RX from remote to spa control box

Several related projects report simple UART:

- 9600 baud
- 8 data bits
- no parity
- 1 stop bit
- short 4-byte frames
- likely frame format: `0xA5 <command> <value> <checksum>`
- likely checksum: low byte of `0xA5 + command + value`

Do **not** hard-code this as guaranteed for all models. Implement the code so the protocol can be confirmed and adjusted after sniffing.

## Supported target systems

There will be two physical spa installations:

1. Home Assistant installation at home
   - Preferred firmware: ESPHome.
   - Expose sensors/switches/numbers directly to Home Assistant.
   - Keep restart-after-power-loss logic local in ESP32 where practical.

2. Homey Pro installation at another site
   - Use the same MSpa protocol/core logic.
   - Preferred interface: local HTTP API and/or MQTT.
   - Avoid depending on Home Assistant/ESPHome for the Homey unit.
   - Homey should be able to call simple local endpoints such as `/api/heater/on`, `/api/filter/on`, `/api/status`.

Architect the code so the MSpa UART protocol layer can be reused by both firmware targets.

## Desired repository structure

Prefer this structure unless the existing repository already has a better one:

```text
/
├── AGENTS.md
├── README.md
├── docs/
│   ├── hardware.md
│   ├── protocol.md
│   ├── home-assistant.md
│   ├── homey-pro.md
│   └── safety.md
├── firmware/
│   ├── common/
│   │   ├── mspa_protocol.h
│   │   ├── mspa_protocol.cpp
│   │   └── README.md
│   ├── esphome-ha/
│   │   ├── mspa-mist-ha.yaml
│   │   └── components/
│   │       └── mspa/
│   └── homey-http/
│       ├── platformio.ini
│       └── src/
│           └── main.cpp
└── tools/
    ├── serial_sniffer.py
    └── decode_frames.py
```

## Development priorities

### Phase 1: Passive sniffer

Build a passive UART sniffer first.

Requirements:

- Read both directions if possible:
  - spa control box → remote
  - remote → spa control box
- Log timestamped raw frames.
- Print both hex and decoded interpretation where known.
- Unknown commands must be logged, not discarded.
- Include a safe hardware wiring guide.
- Provide a Python decode tool that can read a log file and summarize commands.

### Phase 2: Protocol model

Create a robust parser/generator for likely 4-byte frames.

The common protocol module should include:

- Frame validation
- Checksum calculation
- Command enum or named constants
- Unknown frame handling
- Unit tests for frame parsing and checksum
- No UVC/ozone controls enabled by default

Likely command names to support if confirmed:

- power
- heater
- filter
- bubbles
- target temperature
- current temperature/status
- error/status/flow state

UVC and ozone may exist on other models, but for this user’s MSpa Mist they should be optional feature flags and disabled by default.

### Phase 3: Command injection

Only implement command injection after passive sniffing is in place.

Requirements:

- Allow TX to spa control box only through a clearly defined output pin.
- Include a software lockout so injection can be disabled.
- Avoid command collisions if the original remote remains connected.
- Prefer a “send command once, then wait for status confirmation” pattern.
- Log every injected frame.

### Phase 4: Home Assistant firmware

ESPHome target should expose:

- `sensor` current water temperature, if decoded
- `number` target temperature
- `switch` heater
- `switch` filter
- `switch` bubbles
- `binary_sensor` online/status
- `text_sensor` raw status/error
- `button` restart desired mode
- optional `switch` auto_restore_enabled

Local restore behavior:

- On ESP32 boot, wait a configurable delay, default 60 seconds.
- Confirm spa is online if status frames are available.
- Restore only configured safe state:
  - filter on
  - target temperature set
  - heater on
- Do not enable bubbles automatically unless explicitly configured.
- Do not enable heater if flow/error status indicates unsafe operation.

### Phase 5: Homey Pro firmware

Homey target should expose local HTTP and optionally MQTT.

Suggested HTTP endpoints:

```text
GET  /api/status
POST /api/filter/on
POST /api/filter/off
POST /api/heater/on
POST /api/heater/off
POST /api/bubbles/on
POST /api/bubbles/off
POST /api/target-temperature
POST /api/restore
POST /api/auto-restore/on
POST /api/auto-restore/off
```

Requirements:

- JSON responses.
- Local authentication token or simple shared secret should be supported.
- The token must not be hard-coded in committed source.
- Store configuration in a local config file or compile-time secret.
- Include examples for Homey flows using HTTP requests and/or MQTT.

## Code style

- Prefer small, explicit functions.
- Keep hardware access separated from protocol logic.
- Avoid magic numbers in business logic. Put frame IDs and constants in one place.
- Use clear logging with raw hex frames.
- Comments should explain safety decisions and protocol assumptions.
- Keep README and docs practical, with wiring diagrams/tables.

## Testing

When adding or modifying protocol code:

- Add or update parser/checksum unit tests.
- Include examples of raw frames and expected decoding.
- Test unknown frames.
- Test invalid checksum behavior.
- Test disabled optional features such as UVC/ozone.

For PlatformIO firmware:

```bash
pio test
pio run
```

For Python tools:

```bash
python3 -m pytest
python3 tools/decode_frames.py --help
```

For ESPHome:

```bash
esphome config firmware/esphome-ha/mspa-mist-ha.yaml
esphome compile firmware/esphome-ha/mspa-mist-ha.yaml
```

If the repository does not yet contain test infrastructure, create minimal tests around the protocol parser first.

## Documentation expectations

Maintain these documents:

- `docs/safety.md`: safety warnings and non-goals
- `docs/hardware.md`: pinout, voltage measurement, level shifting, wiring examples
- `docs/protocol.md`: discovered UART frames and decoding table
- `docs/home-assistant.md`: ESPHome setup and HA automations
- `docs/homey-pro.md`: HTTP/MQTT setup and Homey flow examples
- `README.md`: quick overview and current status

Include a clear “verified on” table:

```text
Model | Remote type | Voltage | UART settings | Status
MSpa Mist | wired remote | TBD | TBD | In progress
MSpa Muse-like | reference only | TBD | 9600 8N1 reported | Reference
```

## Non-goals

- Do not design a mains power switching system for the spa.
- Do not bypass MSpa safety interlocks.
- Do not require cloud services.
- Do not make UVC/ozone default features for the Mist model.
- Do not assume all MSpa models use the same protocol without sniffing.

## Preferred implementation approach

1. Inspect existing files first.
2. If starting from scratch, create docs and passive sniffer before control firmware.
3. Implement shared protocol parser/generator.
4. Build ESPHome firmware for Home Assistant.
5. Build PlatformIO HTTP/MQTT firmware for Homey Pro.
6. Add examples and tests.
7. Keep all assumptions visible in `docs/protocol.md`.

## User preferences for this project

- The solution should be practical and robust, not over-engineered.
- Home Assistant integration should be clean and entity-based.
- Homey Pro integration should be simple to use with flows.
- The main real-world use case is automatic restore after power loss.
- The original wired remote should preferably still be usable, at least during development/sniffing.
