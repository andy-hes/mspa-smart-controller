# Protocol Discovery

## Status

Denne versjonen bruker etablert grunnlag fra:
- https://www.worldhack.de/your-mspa-goes-smart-step-by-step-to-a-diy-smart-home-hot-tub-wi-fi-upgrade/
- https://github.com/eriktack/esphome_mspacontroller

Mappingen under er derfor referansebasert, men skal fortsatt behandles modellspesifikt.

## Referansemodell

Begge kildene beskriver:
- 4-byte rammer
- format: `0xA5 <command> <value> <checksum>`
- checksum: lavbyte av `0xA5 + command + value`

## Kjent dekodingstabell (fra referanser)

Command | Name | Note
--- | --- | ---
0x01 | heater | on/off (remote -> spa)
0x02 | filter | on/off (remote -> spa)
0x03 | bubbles | nivå 0-3 (remote -> spa)
0x04 | target_temperature | setpoint, noen modeller bruker x2
0x06 | current_temperature | fra spa, verdi/2 i C
0x08 | bath_status | observert 0x00/0x03 i referanse
0x0B | reset | brukt ved reset/flow error i referanse
0x0D | jet | on/off (modellavhengig)
0x0E | ozone | on/off (optional)
0x15 | uvc | on/off (optional)
0x16 | heartbeat | sett i ESPHome-referansen

UVC/ozone er ikke aktivert som standard for Mist-modell.

## Eksempelrammer

- `A5 02 01 A8` -> filter on
- `A5 02 00 A7` -> filter off
- `A5 01 01 A7` -> heater on
- `A5 06 4B F6` -> current temp 37.5 C (0x4B / 2)

## Neste steg

- Lage ESPHome-profil for MSpa Mist med disse kodene
- Holde UVC/ozone disabled som default i all konfig
- Verifisere target-temperature multiplier (x1 vs x2) på aktuell installasjon
