# Home Assistant (ESPHome)

Denne firmwarevarianten er for Home Assistant via ESPHome og er verifisert mot MSpa Mist-logikken i repoet:

- `firmware/esphome-ha/mspa-mist-ha.yaml`
- `firmware/esphome-ha/mspa_tm1650_display.h`

For modeller som faktisk har UVC/Ozone finnes også:
- `firmware/esphome-ha/mspa-uvc-ozone-test.yaml`

## Implementert funksjonalitet

ESPHome-firmwaren har nå:

- temperaturavlesning fra `0x06`
- target temperature som `number`
- `switch` for heater, filter og bubbles
- `binary_sensor` for online og heater-kall
- `text_sensor` for rå status
- `button` for restore desired mode
- `switch` for auto restore
- TM1650-display på eget remote-PCB
- PCB-knapper for lokal styring
- PCB-status-LED-er

### Verifisert PCB-mapping

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

Displayet bruker den verifiserte `TM1650`-strategien:

- `Wire`-basert styring
- normal mapping `DIO=GPIO23`, `CLK=GPIO22`
- boot viser `888`
- idle dimmes
- full lysstyrke i 10 sekunder etter knappetrykk

Visning:

- `OFF` når spa ikke er online
- `Con` når temperatur ennå ikke er kjent
- temperatur med ett desimalpunkt under normal drift

## Restore-logikk

Denne firmwaren er ment å løse strømbruddscenarioet uten å starte feil funksjoner:

- venter som standard `60` sekunder etter boot
- krever online-status før restore
- gjenoppretter lagret ønsket tilstand
- bobler starter ikke automatisk med mindre de faktisk var lagret som på
- heater aktiveres bare når status ser trygg ut

Viktig:
- `desired_*`-verdiene er den lagrede ønskede tilstanden
- aktuell RX-status brukes som feedback, ikke som ukritisk oppstartskilde

## Installering i ESPHome Builder

Dette er den anbefalte måten når du kjører ESPHome som add-on i Home Assistant.

### 1. Kopier filer

Legg disse filene inn i ESPHome-konfigområdet ditt:

- `mspa-mist-ha.yaml`
- `mspa_tm1650_display.h`

De bør ligge i samme mappe, fordi YAML-filen bruker:

```yaml
esphome:
  includes:
    - mspa_tm1650_display.h
```

### 2. Legg inn secrets

Firmwarefilen forventer disse verdiene i `secrets.yaml`:

- `wifi_ssid`
- `wifi_password`
- `fallback_ap_password`
- `api_encryption_key`
- `ota_password`

### 3. Valider

I ESPHome Builder:

1. åpne noden
2. lim inn YAML
3. sørg for at headerfila ligger riktig
4. trykk `Validate`

Hvis Builder klager på manglende header eller metoder i displayklassen, er det nesten alltid fordi `mspa_tm1650_display.h` i Home Assistant ikke er oppdatert til siste versjon.

### 4. Flash første gang

Første flash bør gjøres via USB/seriell i ESPHome Builder.

Etter første flash kan du bruke OTA.

## Hva du bør teste etter flash

1. ved boot:
   - vises `888` tydelig?
2. etter noen sekunder:
   - dimmes displayet?
3. trykk en PCB-knapp:
   - går displayet til full styrke?
4. heater/filter/bubbles-knapper:
   - oppdateres både display og HA-entities?
5. status-LED:
   - følger de forventet funksjon?
6. strøm av/på:
   - restore skjer til lagret ønsket tilstand
   - bobler starter ikke hvis de var av

## Dashboard

Ferdige dashboardfiler finnes i `docs/`:

- `docs/home-assistant-dashboard.yaml`
- `docs/home-assistant-dashboard-view.yaml`
- `docs/home-assistant-dashboard-visual-view.yaml`
- `docs/home-assistant-dashboard-visual-sections-wide.yaml`

Hvis entity-IDene dine avviker fra standard, må dashboardfilene justeres tilsvarende.

## Videre automasjon i Home Assistant

Pakke for restore-logikk:

- `home-assistant/packages/mspa_restore.yaml`

Denne er nyttig hvis du vil kombinere lokal ESP32-restore med HA/Node-RED-logikk for effektstyring eller driftspolitikk.
