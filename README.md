# MSpa Smart Controller

ESP32-basert kontroller/sniffer for MSpa boblebad med kablet fjernkontroll (fokus: MSpa Mist/Muse-lignende).

Dette prosjektet bygger på praktiske felttester med MSpa Mist og har to firmware-mål:
- ESPHome (Home Assistant)
- Lokal HTTP-firmware (Homey Pro)

## Viktig sikkerhet

- Kun lavspenningsgrensesnitt (kabel til kablet remote).
- Ingen forslag om inngrep på nettspenning.
- ESP32 GPIO er ikke 5 V-tolerant: bruk nivåtilpasning eller galvanisk isolasjon.
- ESP32 GPIO er ikke 5 V-tolerant. Bruk alltid nivåbeskyttelse på RX-linjen fra spa.
- Ingen bypass av spaets sikkerhetsfunksjoner (flow/termisk/beskyttelser).
- Ingen inngrep på nettspenningsside.

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
MSpa Mist | wired remote | 5V linje observert på buss | 9600 8N1 | Felt-testet
MSpa Muse-like | reference only | TBD | 9600 8N1 reported | Reference

## Viktigste funn fra felt-test

- Rammer er 4-byte i format `A5 <cmd> <val> <chk>`.
- Stabil styring krever periodisk "hold" av kontrollrammer.
- Hold-intervall på ca `3000 ms` er verifisert stabilt.
- Verifisert hold-sekvens:
  - `0x02` filter
  - `0x01` heater
  - `0x03` bubbles
  - `0x04` target temp
  - `0x16` heartbeat
- `heater` må kjøres sammen med filter for stabil drift.
- `bubbles` kan aktiveres uten heater.
- `0x08` ser ut til å være driftstatus:
  - `0x00` = idle
  - `0x03` = running
- `0x06` er bekreftet som temperatur (`val / 2.0` C).

Detaljer: se [docs/protocol.md](docs/protocol.md).

## Quick start

1. Les sikkerhetsdokumentasjon før tilkobling.
2. Bruk protokollkartet i `docs/protocol.md` (basert på referanseprosjekter).
3. Verifiser mot din spa-modell og hold UVC/ozone deaktivert som standard.
4. Start med `firmware/mspa-lab-sniffer` hvis du vil gjenta/utvide protokolltestene.

## Implementering

- Home Assistant ESPHome firmware:
  - `firmware/esphome-ha/mspa-mist-ha.yaml`
- MSpa lab sniffer/debug firmware:
  - `firmware/mspa-lab-sniffer/platformio.ini`
  - `firmware/mspa-lab-sniffer/src/main.cpp`
  - `docs/mspa-lab-sniffer.md`
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
- [x] Homey HTTP firmware med stabil hold-logikk (3s) og heartbeat
- [x] ESPHome firmware med stabil hold-logikk (3s) og temperaturstyrt heater
- [ ] Langtidstest med original fjernkontroll permanent parallelt tilkoblet

## Hvordan testene ble utført

1. Sniffer/debug firmware koblet direkte på remote-bussen.
2. Manuelle kommandoer via web API (`filter/heater/bubbles/target`).
3. Profil-test med varierende hold-interval, target og heartbeat.
4. Sammenligning av RX-status før/under/etter kommando.
5. Verifisering av stabil drift kun når hold-sekvens ble repetert periodisk.

Se også:
- [docs/mspa-lab-sniffer.md](docs/mspa-lab-sniffer.md)
- [docs/home-assistant.md](docs/home-assistant.md)
- [docs/homey-pro.md](docs/homey-pro.md)

## Remote PCB Test Firmware (ESP32 38P)

For bring-up av eget fjernkontroll-kretskort:
- Firmware: `firmware/remote-pcb-test/`
- Guide: `docs/remote-pcb-bringup.md`
- Signalmapping: `docs/remote-pcb-firmware-signals.md`

Bygg:
```powershell
python -m platformio run -d firmware/remote-pcb-test
```
