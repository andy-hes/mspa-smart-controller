# Homey App (MSpa)

Første versjon av lokal Homey-app som snakker med `firmware/homey-http` via HTTP.

## Hva appen gjør

- Oppretter en Homey-enhet for MSpa-kontrolleren
- Leser status fra `GET /api/status` hvert 10. sekund
- Styrer:
  - Heater (`onoff`)
  - Filter (`mspa_filter`)
  - Bubbles (`mspa_bubbles`)
  - Target temp (`target_temperature`)
  - Auto-restore (`mspa_auto_restore`)
- Flow action: `Restore MSpa desired mode`

## Forutsetninger

1. ESP-firmware fra `firmware/homey-http` kjører og er tilgjengelig på LAN.
2. Homey kan nå ESP-IP på nettverket.

## Installasjon (lokalt utviklingsløp)

Kjør i `homey-app`:

```bash
homey app run
```

Ved pairing:
- Sett `host` til f.eks. `192.168.1.50` (evt `192.168.1.50:80`)

## Begrensninger

- Ingen MQTT i appen ennå
- Ingen avansert feilkode-dekoding utover `bath_status`
- UVC/Ozone er bevisst ikke eksponert i denne Mist-fokuserte versjonen
