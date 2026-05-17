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

## PCB-anbefaling: egen remote-erstatning

Hvis du vil erstatte original fjernkontroll med eget kort, anbefales en modulær oppbygning:

### 1) Hovedblokker

- MCU/Wi-Fi:
  - `ESP32-WROOM-32E` (god støtte, rimelig, robust)
- Strøm:
  - 5V inn fra spa-remote-kabel (hvis verifisert stabil)
  - buck/LDO til 3.3V med god margin
- Bussgrensesnitt:
  - UART RX/TX med nivåtilpasning og seriebeskyttelse
- Brukergrensesnitt:
  - 6 taktile knapper: Heater, Filter, Bobler, Timer, Temp+, Temp-
  - 3-sifret 7-segment (med driver)
  - buzzer (valgfritt)

### 2) Komponenter du typisk trenger

- ESP32-modul:
  - Espressif `ESP32-WROOM-32E`
- 3.3V regulator:
  - f.eks. `AP2112K-3.3` eller buck hvis 5V-linjen er støyete
- Nivåtilpasning SPA->ESP (kritisk):
  - enkel løsning: motstandsdelere per inngang + seriemotstand
  - robust løsning: dedikert logic level translator
- ESD/overspenning:
  - TVS-diode på kabelinngang
  - ESD-beskyttelse på knapper/IO-linjer
- Knapper:
  - 6x IP67-kompatible taktile eller standard taktile med tett frontmembran
- Display:
  - 3-sifret 7-segment + driver (f.eks. TM1637-lignende løsning)
- Programmering/debug:
  - USB-UART header (TX/RX/GND/3V3/EN/IO0)
  - reset/boot-knapper
- Mekanisk:
  - JST-kontakt matchende spa-kabel
  - pakning/silikonmembran for fukt
  - konformal coating (unntatt kontakter/knapper)

### 3) Viktige designvalg

- RX-beskyttelse først:
  - ESP32 skal aldri se direkte 5V på GPIO.
- Grounding:
  - stjernejord mellom strøm, UART og displaydriver.
- EMC/støy:
  - korte UART-spor, jordplan, avkoblingskondensatorer nær alle IC-er.
- Servicevennlig:
  - testpunkter for GND/5V/3V3/UART RX/TX.

### 4) Firmware-strategi for dette kortet

- Samme hardware skal kunne flashes med:
  - ESPHome-variant (Home Assistant)
  - Homey HTTP-variant
- Hold protokollkjerne felles:
  - periodic hold (3s)
  - filter/heater-avhengighet
  - temp-betinget heater

### 5) Fjernkontroll parallelt vs erstatning

- Parallell med original remote:
  - mulig, men må langtidstestes for busskollisjon.
- Full erstatning:
  - enklere deterministisk styring
  - mindre risiko for kolliderende kommandoer
  - anbefalt sluttmål hvis stabilitet er viktigst.
