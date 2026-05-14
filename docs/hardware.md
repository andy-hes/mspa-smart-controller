# Maskinvare

## Omfang og sikkerhet

Denne guiden gjelder lavspenningsgrensesnittet mellom MSpa-kontroller og kablet fjernkontroll.
Ikke gjør inngrep på nettspenning (230V/120V).

Kritisk:
- ESP32 GPIO er ikke 5V-tolerant.
- Ikke koble spa-signaler direkte til ESP32 før nivå er verifisert.
- Behold originale sikkerhetsfunksjoner i spaet.

## Forventet kabel til fjernkontroll (må verifiseres)

Typisk 4-leders kabel:
- GND
- VCC (ofte 5V)
- TX (spa -> fjernkontroll)
- RX (fjernkontroll -> spa)

## Anbefalte ESP32-pinner (dette prosjektet)

Basert på firmware:
- ESP32 RX: `GPIO16`
- ESP32 TX: `GPIO17`
- UART: `9600 8N1`

## Koblingsprinsipp (sikkert)

### 1) Jord
- Koble `MSpa GND` til `ESP32 GND`.

### 2) Spa TX -> ESP32 RX (obligatorisk nivåbeskyttelse)
- `MSpa TX` må ned til 3.3V logikk før `GPIO16`.
- Bruk enten:
  - enveis nivåskifter, eller
  - enkel spenningsdeler (f.eks. 10k/20k) hvis signalet er stabil UART-TTL.

### 3) ESP32 TX -> Spa RX
- `GPIO17` kan ofte leses som HIGH av 5V-TTL-inngang, men dette må verifiseres.
- Hvis spaet ikke leser 3.3V som HIGH, bruk aktiv nivåskifter opp til korrekt nivå.

### 4) VCC / strøm
- Ikke anta at spaets 5V-linje kan drive ESP32 stabilt.
- Anbefalt: egen stabil 5V/3.3V-forsyning (eller godt dimensjonert regulator) til ESP32.

## Praktisk koblingstabell

MSpa-linje | Retning | Via | ESP32
--- | --- | --- | ---
GND | felles | direkte | GND
TX | spa -> esp | nivåskifter ned | GPIO16 (RX)
RX | esp -> spa | direkte eller nivåskifter opp | GPIO17 (TX)
VCC | strøm | kun hvis verifisert | VIN/5V (valgfritt)

## Oppstartsprosedyre

1. Mål først alle linjer med multimeter (hvilenivå).
2. Bekreft signalnivå med logikkanalysator/oscilloskop.
3. Start med kun RX tilkoblet (lesing), ingen TX.
4. Verifiser at ESP ser gyldige 4-byte rammer.
5. Aktiver TX først etter at RX-banen er stabil og korrekt verifisert.

## Vanlige feil

- Ingen data:
  - feil GND-referanse
  - byttet RX/TX
  - feil baudrate
- Ustabil data/checksum-feil:
  - manglende nivåtilpasning
  - støy/jordproblem
- ESP restarter:
  - svak strømforsyning

## Minste stykk-liste

- ESP32 utviklingskort
- Logikknivåskifter (3.3V <-> 5V) eller motstandsnett for RX-banen
- Dupont-ledninger / terminaladapter
- Logikkanalysator (anbefalt)
- Multimeter

## Prosjektnotater

- Home Assistant-firmware forventer UART på `GPIO16/17`.
- UVC/Ozon er ikke aktivert som standard i Mist-oppsettet.
- Auto-restore-logikk kjører lokalt på ESP32, men skal kun brukes etter trygg statusbekreftelse.
