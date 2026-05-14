# Safety

## Scope

Dette prosjektet skal bruke kun lavspenningsgrensesnittet mot kablet MSpa-remote.

## Critical warnings

- Ikke arbeid på nettspenningsside (AC mains) i spa-kontrolleren.
- Ikke bypass eksisterende sikkerhetsfunksjoner: temperaturvern, flow-vern, GFCI/RCD, feiltilstander.
- ESP32 GPIO er ikke 5 V-tolerant. Mål spenning først, og bruk nivåtilpasning/opto-isolasjon.
- Start alltid med passiv sniffing før noe aktiv styring.
- Ved reboot/power restore: ikke slå på heater blindt uten bekreftet trygg status.

## Non-goals

- Ingen design av nettspennings-bryterløsning.
- Ingen deaktivering av produsentens interlocks.

## Current phase

Fase 1-2:
- Passiv UART-sniffing
- Dokumentasjon av protokoll
- Parser/generator og tester

Aktiv command injection er ikke i scope i denne versjonen.
