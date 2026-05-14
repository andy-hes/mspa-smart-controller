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

## Automasjon for drift og strømbrudd

Eksempelfil:
- `home-assistant/packages/mspa_restore.yaml`

Denne automasjonen:
- gjenstarter spa etter strømbrudd/offline når spaet **skal** være i drift
- respekterer manuell avskrudd tilstand (HA eller fjernkontroll)
- respekterer effektbegrensning fra energilogikk (Node-RED)

### Prinsipp

- `input_boolean.mspa_should_run`:
  - lagrer brukerens driftsintensjon (spa skal være i gang)
- `input_boolean.mspa_blocked_by_power`:
  - settes av Node-RED når stor last må kuttes
- `timer.mspa_restore_guard`:
  - hindrer at manuell avstenging mistolkes som feiltilstand rett etter restore

### Legg inn i Home Assistant

1. Opprett mappe `packages` hvis den ikke finnes:
   - `/config/packages/`
2. Kopier filen:
   - fra repo: `home-assistant/packages/mspa_restore.yaml`
   - til HA: `/config/packages/mspa_restore.yaml`
3. Aktiver packages i `configuration.yaml` (hvis ikke allerede aktivert):

```yaml
homeassistant:
  packages: !include_dir_named packages
```

4. Restart Home Assistant.
5. Verifiser at disse helperne finnes:
   - `input_boolean.mspa_should_run`
   - `input_boolean.mspa_blocked_by_power`
   - `timer.mspa_restore_guard`

### Node-RED kobling (effektbegrensning)

Når Node-RED kutter store laster:
- sett `input_boolean.mspa_blocked_by_power` til `on`

Når effektbegrensning oppheves:
- sett `input_boolean.mspa_blocked_by_power` til `off`

Anbefalt service-kall i Node-RED:
- `input_boolean.turn_on` / `input_boolean.turn_off`
- target: `input_boolean.mspa_blocked_by_power`
