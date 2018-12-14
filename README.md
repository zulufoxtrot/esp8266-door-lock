# RFID/Web door lock

![Photo](photo.jpg?raw=true)

Features:

- Works with Mifare classic smartcards
- Works with phone NFC chips (with a hack)
- On-demand card assignment
- Remote locking/unlocking through HTTP API
- OTA updates


## Credits

Adapted from Omer Siar Baysal's [access control library](https://github.com/omersiar/RFID522-Door-Unlock/blob/master/AccessControl/AccessControl.ino).

## Wiring

Marche pour le nodemcu 0.9
- 3.3V------ 3.3V
- RST ------ (D2) - GPIO4
- GND------- GND
- IRQ -----
- MISO -------(D6) - GPIO12 - HMISO
- MOSI -------(D7) - GPIO13 - RXD2 - HMOSI
- SCK --------(D5) - GPIO14 - HSCLK
- SS ----------(D4) - GPIO2 - TXD1

Alimentation:

Fonctionne avec 500mA normalement. Peut-être 1A pour être sûr.

## Procédures

Pour ajouter une nouvelle carte, il faut:
- passer la master card
- passer la carte à ajouter
- repasser la master card pour valider.

Pour enlever une carte:
- passer la master card
- passer la carte à enlever
- repasser la master card pour valider.

