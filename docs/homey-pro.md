# Homey Pro

Ferdig første versjon:
- `firmware/homey-http/platformio.ini`
- `firmware/homey-http/src/main.cpp`

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

Konfigurasjon uten hardkodede secrets:
- `MSPA_WIFI_SSID`
- `MSPA_WIFI_PASSWORD`

Eksempel (PowerShell før build):
```powershell
$env:MSPA_WIFI_SSID="ditt-ssid"
$env:MSPA_WIFI_PASSWORD="ditt-passord"
pio run -d firmware/homey-http
```

Sikkerhetslogikk:
- auto-restore etter boot (60 sek default)
- heater tvinges av hvis filter er av
- status/temperature leses fra UART (`0x08`, `0x06`)
- UVC/Ozone ikke aktivert i denne Mist-fokuserte varianten

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
