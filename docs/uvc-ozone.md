# UVC/Ozone: Funn, antakelser og testplan

## Kort status

- UVC/Ozone er **ikke verifisert** på MSpa Mist (din modell).
- De er behandlet som **valgfrie funksjoner** for andre modeller.
- Standard for Mist skal fortsatt være: **UVC/Ozone av**.

## Forelopig antatt kommandomapping

Basert pa referanser og eksisterende protokollarbeid i prosjektet:

- `0x0E` -> Ozone on/off (antatt)
- `0x15` -> UVC on/off (antatt)

Format:

- `A5 <cmd> <val> <chk>`
- `chk = (A5 + cmd + val) & 0xFF`

Eksempler:

- Ozone ON: `A5 0E 01 B4`
- Ozone OFF: `A5 0E 00 B3`
- UVC ON: `A5 15 01 BB`
- UVC OFF: `A5 15 00 BA`

## Hva som er implementert na

- Egen sniffer/test-firmware med UVC/Ozone-presets:
  - `firmware/mspa-lab-sniffer-uvc-ozone/`
- Egen ESPHome testprofil for modeller med UVC/Ozone:
  - `firmware/esphome-ha/mspa-uvc-ozone-test.yaml`

Begge er ment som **valideringsverktoy** for modeller som faktisk har disse funksjonene.

## Viktige sikkerhetspunkter

- Ikke aktiver UVC/Ozone i auto-restore uten modellspesifikk verifisering.
- Ikke anta at alle MSpa-modeller tolker `0x0E`/`0x15` likt.
- Hold originale sikkerhetsfunksjoner i spa urort.

## Testplan for modell med UVC/Ozone

1. Start med sniffer-only (TX av).
2. Logg normal trafikk i idle/running.
3. Send kun én kommando av gangen:
   - Ozone ON/OFF
   - UVC ON/OFF
4. Bekreft endring i RX-status (kode/verdi som faktisk endrer seg).
5. Aktiver hold kun etter at kommando og statusmapping er bekreftet.

## Malkrav for "verifisert"

- Kommando gir repeterbar fysisk effekt.
- Effekt kan stoppes med tilsvarende OFF-kommando.
- RX-status har stabil korrelasjon med funksjon on/off.
- Ingen konflikt med heater/filter sikkerhetslogikk.

