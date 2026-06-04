# Homey Pro / Ren Wi-Fi firmware

Denne firmwarevarianten er laget for en ren ESP32-løsning uten ESPHome, med lokal webside, HTTP API og MQTT:

- `firmware/homey-http/platformio.ini`
- `firmware/homey-http/src/main.cpp`
- `firmware/homey-http/include/mspa_tm1650_display.h`

Den er ment for Homey Pro, men kan også brukes helt uten Homey hvis du bare vil styre lokalt via HTTP eller MQTT.

## Implementert funksjonalitet

Wi-Fi-firmwaren har nå:

- lokal webside på ESP32
- lokal HTTP API for status og styring
- MQTT-klient
- MQTT-konfigurasjon via webgrensesnitt
- valgfri ozone/UVC-støtte
- TM1650-display på eget remote-PCB
- PCB-knapper for lokal styring
- PCB-status-LED-er
- restore-logikk etter strømbrudd

## Verifisert PCB-mapping

- display:
  - `DIO = GPIO23`
  - `CLK = GPIO22`
- knapper:
  - `GPIO13` mode/restore
  - `GPIO32` heater
  - `GPIO33` filter
  - `GPIO26` auto-restore
  - `GPIO25` bubbles
  - `GPIO14` temp down
  - `GPIO27` temp up
- LED:
  - `GPIO21` filter aktiv
  - `GPIO19` heater-funksjon aktiv
  - `GPIO18` bubbles aktiv
  - `GPIO5` feil/offline
  - `GPIO4` aktiv oppvarming

## Displayoppførsel

Displayet bruker samme verifiserte strategi som PCB-testfirmwaren og ESPHome:

- `TM1650` over `Wire`
- `DIO=GPIO23`
- `CLK=GPIO22`
- `888` ved boot
- full styrke i 10 sekunder etter knappetrykk
- dimmet idle-visning etterpå

Tekster:

- `AP ` når fallback hotspot er aktiv
- `rSt` mens restore fortsatt venter
- `OFF` når spa ikke er online
- `Con` når temperatur ennå ikke er kjent
- temperatur med ett desimalpunkt ved normal drift

## HTTP API

Støttede endepunkt:

- `GET /api/status`
- `POST /api/filter/on`
- `POST /api/filter/off`
- `POST /api/heater/on`
- `POST /api/heater/off`
- `POST /api/bubbles/on`
- `POST /api/bubbles/off`
- `POST /api/ozone/on`
- `POST /api/ozone/off`
- `POST /api/uvc/on`
- `POST /api/uvc/off`
- `POST /api/target-temperature`
- `POST /api/restore`
- `POST /api/auto-restore/on`
- `POST /api/auto-restore/off`
- `POST /api/wifi`
- `POST /api/mqtt`
- `POST /api/features`

`/api/status` returnerer JSON med blant annet:

- online
- current temperature
- target temperature
- filter/heater/bubbles
- auto restore
- bath status
- optional status `0x0E` og `0x15`
- Wi-Fi-status
- MQTT-status

## Ozone / UVC

Stotte for ozone og UVC er lagt inn for modeller som faktisk har disse funksjonene, men er fortsatt av som standard for MSpa Mist.

Brukte kommandorammer:

- `0x0E` = ozone on/off
- `0x15` = UVC on/off

Aktivering skjer fra webgrensesnittet under `Optional Features`.

Når støtte er aktivert:

- webgrensesnittet viser knapper for ozone og/eller UVC
- HTTP API-et godtar `POST /api/ozone/on|off` og `POST /api/uvc/on|off`
- MQTT publiserer og abonnerer også på disse funksjonene

Når støtte ikke er aktivert:

- firmwaren sender ikke `0x0E` eller `0x15`
- API-kall til disse endepunktene returnerer feil

Dette er bevisst, slik at Mist-oppsett ikke skal få eksperimentelle funksjoner aktivert ved en feil.

## MQTT

MQTT kan aktiveres og konfigureres fra webgrensesnittet.

Felt som lagres i ESP32:

- enabled
- host
- port
- username
- password
- base topic

### Publiserte topics

Ut fra `base_topic`, for eksempel `mspa/controller`:

- `mspa/controller/availability`
- `mspa/controller/status`
- `mspa/controller/state/filter`
- `mspa/controller/state/heater`
- `mspa/controller/state/bubbles_level`
- `mspa/controller/state/ozone`
- `mspa/controller/state/uvc`
- `mspa/controller/state/target_temperature_c`
- `mspa/controller/state/current_temperature_c`
- `mspa/controller/state/online`
- `mspa/controller/state/auto_restore_enabled`

### Kommando-topics

- `mspa/controller/command/filter/set`
- `mspa/controller/command/heater/set`
- `mspa/controller/command/bubbles/set`
- `mspa/controller/command/ozone/set`
- `mspa/controller/command/uvc/set`
- `mspa/controller/command/target_temperature/set`
- `mspa/controller/command/auto_restore/set`
- `mspa/controller/command/restore`

Payload:

- boolske kommandoer godtar `on/off`, `true/false`, `1/0`, `yes/no`
- `bubbles/set` forventer `0..3`
- `target_temperature/set` forventer `15..40`

For `ozone/set` og `uvc/set` brukes boolsk payload, for eksempel `on` eller `off`.

## Restore-logikk

Restore i denne firmwaren er oppdatert til å matche ESPHome-retningen:

- venter standard `60` sekunder etter boot
- sender bare heartbeat mens `restore_pending` er aktiv
- bruker lagret ønsket tilstand som sann kilde
- bobler starter ikke automatisk hvis de ikke var lagret som på
- filter aktiveres hvis heater eller bubbles krever det
- heater aktiveres bare når temperatur og ønsket tilstand tilsier det

## Installering

### 1. Bygg

Kjør fra repo-roten eller med `-d` som peker til firmwaremappen:

```powershell
python -m platformio run -d firmware/homey-http
```

### 2. Finn serieport

```powershell
python -m platformio device list
```

### 3. Flash

Eksempel med `COM7`:

```powershell
python -m platformio run -d firmware/homey-http -t upload --upload-port COM7
```

### 4. Seriell monitor

```powershell
python -m platformio device monitor -d firmware/homey-http --port COM7 --baud 115200
```

## Første oppsett etter flashing

### Hvis Wi-Fi allerede er konfigurert

1. finn IP-adressen i seriellmonitoren
2. åpne:

```text
http://<esp-ip>/
```

### Hvis Wi-Fi ikke er konfigurert eller ikke virker

ESP32 starter fallback hotspot:

- SSID: `MSpa-Setup`
- passord: `mspasetup`

Åpne så:

```text
http://192.168.4.1/
```

Der kan du:

- legge inn Wi-Fi SSID/passord
- lagre MQTT-server, port og topic
- aktivere støtte for ozone/UVC hvis modellen din faktisk har det
- se status
- styre spa-funksjonene direkte

## Praktisk Homey-bruk

Det finnes to vanlige måter å bruke denne med Homey Pro på:

### 1. HTTP-basert

Homey Flow eller Homey-app sender lokale `POST`-kall til ESP32:

- slå på filter
- slå på heater
- endre temperatur
- kjøre restore

### 2. MQTT-basert

Homey eller annen lokal automasjon:

- leser status fra MQTT
- sender kommandoer til `command/...`-topicene

MQTT er ofte den reneste løsningen hvis du vil ha både tilstandsoppdateringer og enkel styring uten å polle HTTP-status hele tiden.

## Hva du bør teste etter installasjon

1. boot:
   - vises `888`?
2. webside:
   - svarer `http://<esp-ip>/`?
3. HTTP:
   - virker `/api/status`?
4. knapper:
   - styrer de heater/filter/bubbles/temp?
5. LED:
   - følger de forventet tilstand?
6. optional features:
   - hvis aktivert, virker ozone/UVC fra web?
7. MQTT:
   - kobler den til broker?
   - publiseres status?
   - virker kommando-topics?
8. strøm av/på:
   - restore skjer til lagret ønsket tilstand
   - bobler kommer ikke tilbake hvis de var av
