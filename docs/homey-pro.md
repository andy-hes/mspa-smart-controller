# Homey Pro

Ferdig første versjon:
- `firmware/homey-http/platformio.ini`
- `firmware/homey-http/src/main.cpp`

HTTP-endepunkt (token via header `X-Auth-Token`):
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
- `MSPA_API_TOKEN`

Eksempel (PowerShell før build):
```powershell
$env:MSPA_WIFI_SSID="ditt-ssid"
$env:MSPA_WIFI_PASSWORD="ditt-passord"
$env:MSPA_API_TOKEN="lang-delt-hemmelig-token"
pio run -d firmware/homey-http
```

Sikkerhetslogikk:
- auto-restore etter boot (60 sek default)
- heater tvinges av hvis filter er av
- status/temperature leses fra UART (`0x08`, `0x06`)
- UVC/Ozone ikke aktivert i denne Mist-fokuserte varianten
