# Homey Pro

Ferdig første versjon:
- `firmware/homey-http/platformio.ini`
- `firmware/homey-http/src/main.cpp`
- `firmware/homey-http/flash.ps1`

HTTP-endepunkt (lokalt LAN):
- GET `/api/status`
- POST `/api/filter/on`
- POST `/api/filter/off`
- POST `/api/heater/on`
- POST `/api/heater/off`
- POST `/api/bubbles/on`
- POST `/api/bubbles/off`
- POST `/api/target-temperature` (`value` 20-40)
- POST `/api/restore`
- POST `/api/auto-restore/on`
- POST `/api/auto-restore/off`

## Bygg og flashing til ESP32

PlatformIO er brukt for denne firmwaren. På denne maskinen kan kommandoene kjøres via Python-modulen:

```powershell
python -m platformio run -d firmware/homey-http
```

Finn USB/COM-port:

```powershell
python -m platformio device list
```

Flash til ESP32, eksempel med `COM3`:

```powershell
python -m platformio run -d firmware/homey-http -t upload --upload-port COM3
```

Alternativt kan den ferdige flashfilen brukes:

```powershell
powershell -ExecutionPolicy Bypass -File firmware/homey-http/flash.ps1 -Port COM3
```

Flash og åpne seriell monitor etterpå:

```powershell
powershell -ExecutionPolicy Bypass -File firmware/homey-http/flash.ps1 -Port COM3 -Monitor
```

Seriell monitor uten flashing:

```powershell
python -m platformio device monitor -d firmware/homey-http --port COM3
```

Wi-Fi trenger ikke hardkodes. Første oppsett kan gjøres via fallback hotspot hvis ESP32 ikke finner lagret nettverk.

Sikkerhetslogikk:
- auto-restore etter boot (60 sek default)
- heater tvinges av hvis filter er av
- status/temperature leses fra UART (`0x08`, `0x06`)
- UVC/Ozone ikke aktivert i denne Mist-fokuserte varianten

## Verifisert hold-logikk

For stabil drift mot MSpa Mist sender firmwaren periodisk hold hvert `3000 ms`:
- `A5 02 <filter> <chk>`
- `A5 01 <heater> <chk>`
- `A5 03 <bubbles> <chk>`
- `A5 04 <target_raw> <chk>`
- `A5 16 00 BB` (heartbeat)

Logikk i firmware:
- filter aktiveres automatisk hvis heater eller bubbles er ønsket
- heater holdes kun når filter er aktiv og `current_temp < target_temp`
- target sendes i hver hold-syklus

## ESP32 lokal webside og lagrede innstillinger

Homey-firmwaren har nå lokal webside på ESP32:
- `http://<esp-ip>/`

Websiden viser:
- online/status
- aktuell temperatur
- setpunkt
- filter/heater/bubbles
- auto-restore
- Wi-Fi status/IP

Websiden lar deg:
- styre filter/heater/bubbles
- sette temperatur
- lagre Wi-Fi SSID/passord

ESP32 bruker NVS/Preferences til å lagre:
- Wi-Fi SSID/passord
- filter/heater ønsket tilstand
- ønsket temperatur
- auto-restore aktivert/deaktivert
- om spaet skal være i drift (`desired_run`)

## Fallback hotspot

Hvis ESP32 ikke får koblet til Wi-Fi innen 2 minutter:
- den starter fallback hotspot `MSpa-Setup`
- passord: `mspasetup`
- webside: `http://192.168.4.1/`

Bruk denne siden til å legge inn riktig Wi-Fi.

## Fjernkontroll og auto-restore

Firmware leser statusrammer fra spaet (`0x08`):
- status `0x03` tolkes som at spaet går
- status `0x00` tolkes som av/idle

Hvis spaet går og senere blir slått av via original fjernkontroll:
- ESP32 lagrer `desired_run = false`
- `auto_restore_enabled` blir automatisk slått av
- spaet blir ikke startet igjen etter strømbrudd

Hvis spaet blir slått på igjen via original fjernkontroll:
- ESP32 lagrer `desired_run = true`
- `auto_restore_enabled` blir automatisk aktivert igjen

Det er lagt inn oppstart/restore guard-tid for å unngå at ESP32 mistolker normal reboot/restore som manuell avstenging.

## Teststatus

- Stabil styring er verifisert i direkte bus-test uten original remote tilkoblet.
- Neste anbefalte verifisering er langtidstest med original remote parallelt tilkoblet for å bekrefte kollisjonshåndtering over tid.
- Hvis parallell drift gir ustabilitet, bruk dedikert ESP-basert remote-erstatning.

## Lokal simulering uten ESP32

For å teste Homey-appen før maskinvaren er klar, bruk:
- `tools/simulate_mspa_http.py`

Start simulator:

```bash
python tools/simulate_mspa_http.py --port 8080
```

I Homey-enhetens innstillinger:
- `host`: `<IP-til-PC>:8080`

Eksempel:
- host: `192.168.1.123:8080`

Nyttige test-kall:

```bash
# status
curl http://127.0.0.1:8080/api/status

# simulér offline/online
curl -X POST http://127.0.0.1:8080/api/sim/offline
curl -X POST http://127.0.0.1:8080/api/sim/online

# sett temperatur i simulator
curl -X POST "http://127.0.0.1:8080/api/sim/temp?value=34.5"
```
