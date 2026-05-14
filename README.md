# MSpa Smart Controller

ESP32-basert controller for MSpa boblebad med kablet fjernkontroll (fokus: MSpa Mist/Muse-lignende).

Fokus i denne første versjonen:
- Sikkerhetsdokumentasjon
- Felles protokollgrunnlag fra etablerte referanser
- Felles MSpa-protokollparser med tester

## Viktig sikkerhet

- Kun lavspenningsgrensesnitt (kabel til kablet remote).
- Ingen forslag om inngrep på nettspenning.
- ESP32 GPIO er ikke 5 V-tolerant: bruk nivåtilpasning eller galvanisk isolasjon.
- Aktiv command injection er ikke implementert i denne fasen.

Se:
- [docs/safety.md](docs/safety.md)
- [docs/hardware.md](docs/hardware.md)
- [docs/protocol.md](docs/protocol.md)

## Prosjektstruktur

- `docs/` sikkerhet, hardware, protokoll, HA/Homey-notater
- `firmware/common/` felles protokollkode (C++)
- `tools/` dekoder/utility-verktøy (Python)
- `tests/` parser/dekoder-tester

## Verifisert på

Model | Remote type | Voltage | UART settings | Status
--- | --- | --- | --- | ---
MSpa Mist | wired remote | TBD | TBD | In progress
MSpa Muse-like | reference only | TBD | 9600 8N1 reported | Reference

## Quick start

1. Les sikkerhetsdokumentasjon før tilkobling.
2. Bruk protokollkartet i `docs/protocol.md` (basert på referanseprosjekter).
3. Verifiser mot din spa-modell og hold UVC/ozone deaktivert som standard.

## Implementering

- Home Assistant ESPHome firmware:
  - `firmware/esphome-ha/mspa-mist-ha.yaml`
- Home Assistant dashboard (visuell view):
  - `docs/home-assistant-dashboard-visual-sections-wide.yaml`
- Homey lokal HTTP firmware:
  - `firmware/homey-http/platformio.ini`
  - `firmware/homey-http/src/main.cpp`
- Homey app:
  - `homey-app/`

### Python-verktøy

```bash
python -m pytest
python tools/decode_frames.py --help
```

### C++ parser (lokal test)

```bash
python -m pytest tests/test_mspa_protocol_vectors.py
```

## Status

- [x] Sikkerhets- og hardware-dokumentasjon opprettet
- [x] Protokollgrunnlag fra referanseprosjekter dokumentert
- [x] Felles parser/generator med checksum opprettet
- [x] Grunnleggende enhetstester opprettet
- [ ] Aktiv command injection (bevisst utsatt)
