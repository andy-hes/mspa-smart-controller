# Firmware Display Plan

## Mål

Bruke den verifiserte displayløsningen fra PCB-testfirmwaren i alle tre firmware-spor:

- ESPHome / Home Assistant
- Wi-Fi / HTTP-firmware
- Homey-firmware

Verifisert retning så langt:

- `TM1650`-bibliotek / `Wire`-basert styring virker
- normal mapping virker:
  - `DIO = GPIO23`
  - `CLK = GPIO22`
- byttet `CLK/DIO` virker ikke
- tidligere bitbang-variant skal ikke brukes videre som standard

## Fase 1: ESPHome

Status:
- Startet
- Displayoppdatering er flyttet fra bitbang-lambda til `Wire`-basert helper

Arbeid:
- verifiser `esphome config`
- verifiser `esphome compile`
- test på ekte maskinvare:
  - boottekst
  - offline-tekst
  - connecting-tekst
  - temperaturvisning

Ønsket visning:
- boot: `SPA`
- ikke online: `OFF`
- ingen temp ennå: `Con`
- normal drift: temperatur med ett desimalpunkt

## Fase 2: Wi-Fi / HTTP-firmware

Målfil:
- `firmware/homey-http/src/main.cpp`

Arbeid:
- bytt eksisterende displaykode til samme `Wire`-baserte helperstrategi
- behold samme pinmapping:
  - `GPIO23`
  - `GPIO22`
- vis:
  - boottekst
  - AP/setup-status
  - online/offline
  - temperatur
  - enkel feiltekst

Forslag til visning:
- boot: `SPA`
- AP-modus: `AP `
- offline: `OFF`
- restore pending: `rSt`
- temperatur: numerisk

## Fase 3: Homey-firmware

Mål:
- samme displayhelper skal brukes av den firmwarevarianten som faktisk kjører på Homey-oppkoblet enhet

Arbeid:
- gjenbruk samme helper som i ESPHome/Wi-Fi-sporet
- standardiser displaystater
- unngå tre forskjellige displayimplementasjoner

## Praktisk arkitektur

Anbefalt videre:

1. Lag én felles displayhelper for Arduino/C++-sporene
2. Hold ESPHome med en liten separat helper-header
3. Bruk samme tekster og samme state mapping i alle firmwarevarianter

## Ikke gjør videre

- Ikke gå tilbake til gammel bitbang som standard
- Ikke bytt `CLK/DIO`
- Ikke legg displaylogikk flere steder med ulik oppførsel
