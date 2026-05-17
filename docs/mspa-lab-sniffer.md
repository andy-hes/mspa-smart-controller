# MSpa Lab Sniffer (ESP32)

Dette er et eget test-firmware for trygg protokollkartlegging uten original fjernkontroll tilkoblet.

## Formål

- Sniffe UART-rammer fra MSpa kontinuerlig
- Sende testkommandoer manuelt fra webgrensesnitt
- Logge alt over Wi-Fi i nettleser og serial monitor

## Viktig sikkerhet

- Kun lavspenningslinjene mot remote-kabelen.
- ESP32 GPIO er ikke 5V-tolerant.
- Bruk nivatilpasning på RX-banen (MSpa TX -> ESP RX).
- Ikke gjor inngrep i nettspenningsdelen av spaet.

## Standard pinner

- `UART_RX_PIN=16`
- `UART_TX_PIN=17`
- `UART_BAUD=9600`

## Bygg og flash

```powershell
python -m platformio run -d firmware/mspa-lab-sniffer
python -m platformio device list
powershell -ExecutionPolicy Bypass -File firmware/mspa-lab-sniffer/flash.ps1 -Port COM3
```

Med monitor:

```powershell
powershell -ExecutionPolicy Bypass -File firmware/mspa-lab-sniffer/flash.ps1 -Port COM3 -Monitor
```

## Tilkobling

1. Koble felles GND.
2. Koble MSpa TX til ESP32 RX via nivatilpasning.
3. Koble ESP32 TX til MSpa RX (gjerne med seriemotstand / nivatilpasning).
4. Koble ikke original fjernkontroll samtidig i denne testmodusen.

## Webgrensesnitt

- Enheten starter i STA hvis Wi-Fi finnes.
- Hvis ikke, starter fallback AP:
  - SSID: `MSpa-Lab`
  - passord: `mspalab123`
- Aapne IP i nettleser og bruk:
  - Filter/Heater/Bubbles knapper
  - Set Temp (15-40)
  - Send Raw (`cmd`/`val` i hex)
  - TX Enable / TX Disable (sniff-only)
  - Auto Test START/STEP/STOP med valgfri step-tid (ms)

## API-endepunkter

- `GET /api/status`
- `GET /api/logs`
- `POST /api/send` (`cmd`, `val` i hex)
- `POST /api/tx-enabled` (`value=1|0`)
- `POST /api/preset/filter/on|off`
- `POST /api/preset/heater/on|off`
- `POST /api/preset/bubbles/1|off`
- `POST /api/preset/target` (`value=15..40`)
- `POST /api/preset/heartbeat`
- `POST /api/auto-test/start` (`step_ms=2000..120000`)
- `POST /api/auto-test/step`
- `POST /api/auto-test/stop`
- `POST /api/profile-test/start`
- `POST /api/profile-test/stop`

## Auto test-sekvens

Ved `auto-test/start` kjores denne syklusen med valgt step-tid:

1. `idle_heartbeat` (`0x16 0x00`)
2. `filter_on` (`0x02 0x01`)
3. `heater_on` (`0x01 0x01`)
4. `target_38` (`0x04 0x26`)
5. `bubbles_on` (`0x03 0x01`)
6. `bubbles_off` (`0x03 0x00`)
7. `heater_off` (`0x01 0x00`)
8. `filter_off` (`0x02 0x00`)
9. `target_34` (`0x04 0x22`)
10. `heartbeat` (`0x16 0x00`)

Alle steg logges som `STEP <nr> <navn>` i web-logg og serial.

## Profiltest for hold/mode (anbefalt)

`Profile Test START` kjører en ferdig sekvens som tester:

1. Hold-intervall (`2s`, `3s`, `4s`)
2. Med/uten heartbeat (`0x16`)
3. Med/uten target-temp (`0x04`)
4. Filter/heater/bobler-kombinasjoner

Hvert steg logges som `PROFILE STEP <nr> <navn> ...`.
Dette gjør det enkelt å se hvilke kombinasjoner som holder spaet aktivt stabilt.

## Hva du sender tilbake for analyse

Kjor en kontrollert sekvens (10-20 sek per steg) og send logg:

1. Idle (ingen endring)
2. Filter on/off
3. Heater on/off
4. Bubbles on/off
5. Setpoint opp/ned

Inkluder bade-status samtidig, sa vi kan mappe `0x18`/`0x1A`/`0x12` mot faktiske funksjoner.
