# Sistemes Encastats · Pràctiques

Repositori de pràctiques de Pau Vila · UVic-UCC · curs 2026-27

## Maquinari

| Element | Model | Connexió |
|---|---|---|
| Gateway | Raspberry Pi 4, Raspberry Pi OS Lite 64 bits | Ethernet |
| Node | M5Stack AtomS3 (ESP32-S3) amb pantalla | USB |
| Sensor | DHT22 (AM2302) | Dades a G8, alimentació a 3,3 V |
| LED | LED 5 mm + resistència 220 Ω | G7 |
| Polsador | Polsador + pull-up intern | G6 i GND |

## Gateway

- Usuari: `pau`
- Accés per SSH només amb clau pública
- Node-RED s'executa com a servei i s'obre al port 1880

## Estructura

```
firmware/   programes de l'AtomS3 (un sketch per carpeta)
gateway/    configuració i scripts del Raspberry Pi
flows/      fluxos de Node-RED exportats en JSON
docs/       informes i fotografies
```

## Com engegar-ho tot des de zero

### Raspberry Pi
1. Escriure Raspberry Pi OS Lite (64 bits) a la microSD amb Raspberry Pi Imager, amb SSH per clau pública i l'usuari `pau`.
2. Arrencar-lo connectat per Ethernet i entrar-hi per SSH.
3. Actualitzar el sistema: `sudo apt update && sudo apt full-upgrade -y`
4. Instal·lar Node-RED amb l'script oficial i activar-lo com a servei:
```bash
   bash <(curl -sL https://raw.githubusercontent.com/node-red/linux-installers/master/deb/update-nodejs-and-nodered)
   sudo systemctl enable nodered.service
   sudo systemctl start nodered.service
```
5. Obrir l'editor al navegador (port 1880) i importar els fluxos de `flows/` (menú → Import).

### AtomS3
1. Instal·lar l'IDE Arduino i el paquet de plaques de M5Stack.
2. Instal·lar les llibreries: **DHT sensor library** (Adafruit), **Adafruit Unified Sensor** i **M5Unified**.
3. Placa: **M5AtomS3** · USB CDC On Boot: **Enabled** · Upload Speed: **115200**.
4. Obrir el sketch de `firmware/` i carregar-lo.
5. Si la càrrega falla, posar la placa en mode de descàrrega (mantenir el reset 2 s fins que s'encengui el LED verd) i tornar-ho a provar.
6. Després de carregar, prémer el reset una vegada. Monitor sèrie a 115200 bauds.

## Pràctiques

- **P1** · Muntatge de l'entorn: SSH, Node-RED, LED i polsador, DHT22 i pantalla.
