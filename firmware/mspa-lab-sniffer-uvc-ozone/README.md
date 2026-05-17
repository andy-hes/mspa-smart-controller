# MSpa Lab Sniffer: UVC/Ozone test variant

Denne varianten er for modelltester der UVC/Ozone kan finnes.

## Endepunkter

- `POST /api/preset/ozone/on` -> sender `A5 0E 01 B4`
- `POST /api/preset/ozone/off` -> sender `A5 0E 00 B3`
- `POST /api/preset/uvc/on` -> sender `A5 15 01 BB`
- `POST /api/preset/uvc/off` -> sender `A5 15 00 BA`

Statusfelt i `/api/status`:

- `last_status_0e`
- `last_status_15`

## Bygg/flash

```powershell
python -m platformio run -d firmware/mspa-lab-sniffer-uvc-ozone
python -m platformio run -d firmware/mspa-lab-sniffer-uvc-ozone -t upload
```

## Viktig

Dette er test/verifisering, ikke default for Mist.
Se `docs/uvc-ozone.md` for testplan og sikkerhetsgrenser.

