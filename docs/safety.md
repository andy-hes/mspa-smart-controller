# Safety

## Scope

Dette prosjektet skal bruke kun lavspenningsgrensesnittet mot kablet MSpa-remote.
Løsningen er bygget for å bruke eksisterende kontrollerhus, pakninger og kabel, uten inngrep på spaets hovedkort.

## Critical warnings

- Ikke arbeid på nettspenningsside (AC mains) i spa-kontrolleren.
- Ikke gjør inngrep på spaets hovedkort.
- ESP32 GPIO er ikke 5 V-tolerant.
- Bruk 3.3 V GPIO mot boblebadet og spenningsdeling på RX-linjen fra spa.
- Ved tap av kommunikasjon mellom spa og kontroller skal alle funksjoner falle av.
- Start alltid med passiv sniffing før noe aktiv styring.
- Ved reboot/power restore: ikke slå på heater blindt uten bekreftet trygg status.

## Non-goals

- Ingen design av nettspennings-bryterløsning.
- Ingen inngrep på spaets hovedkort.

## Current phase

Fase 1-2:
- Passiv UART-sniffing
- Dokumentasjon av protokoll
- Parser/generator og tester

Aktiv command injection er ikke i scope i denne versjonen.
