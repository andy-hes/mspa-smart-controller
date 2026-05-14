# Home Assistant (ESPHome)

Ferdig første versjon:
- `firmware/esphome-ha/mspa-mist-ha.yaml`

Innhold:
- sensor: current temperature (fra `0x06` / 2)
- number: target temperature
- switch: heater, filter, bubbles
- binary_sensor: online
- text_sensor: raw status frame
- button: restore desired mode
- switch: auto_restore_enabled

Sikker restore:
- venter default 60 sek etter boot
- krever online-status før restore
- restore: filter on -> target temperature -> heater on
- heater holdes av hvis filter er av
- bubbles restore default av
- UVC/Ozone er ikke eksponert i Mist-profilen

Viktig:
- Sett hemmeligheter i `secrets.yaml`: wifi, api key, ota passord, fallback AP-passord.
- Verifiser spenningsnivå før tilkobling.

## Dashboard

Ferdig dashboard-YAML:
- `docs/home-assistant-dashboard.yaml`

Bruk:
1. Home Assistant -> Settings -> Dashboards -> Add Dashboard.
2. Velg YAML mode dashboard.
3. Lim inn innholdet fra `docs/home-assistant-dashboard.yaml`.

Merk:
- Entity-IDene i dashboardet er basert på standard navngivning fra ESPHome-filen.
- Hvis dine entity-IDer avviker, oppdater disse i dashboard-YAML:
  - `sensor.mspa_current_temperature`
  - `number.mspa_target_temperature`
  - `switch.mspa_filter`
  - `switch.mspa_heater`
  - `switch.mspa_bubbles`
  - `binary_sensor.mspa_online`
  - `switch.mspa_auto_restore_enabled`
  - `button.mspa_restore_desired_mode`
  - `text_sensor.mspa_raw_status`
