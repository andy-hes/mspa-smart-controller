# Common MSpa Protocol Module

Denne mappen inneholder delbar C++-logikk for MSpa frame parsing/generering.

Mål:
- Gjenbruk mellom ESPHome- og Homey-firmware
- Tydelig separasjon mellom transport (UART) og protokoll
- Robust håndtering av ukjente frames

Merk:
- UVC/ozone er feature-flaggede kommandoer og er av som standard.
- Aktiv command injection ligger utenfor denne første versjonen.
