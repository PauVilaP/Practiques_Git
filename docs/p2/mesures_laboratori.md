# P2 · Dades de les proves al laboratori (07/10/2026)

Dades en brut per a l'informe. Node: M5Stack AtomS3 a la Wi-Fi Lab-Modul (IP 10.11.2.22).
Raspberry: Node-RED 4 a 10.11.1.11 (el 05/10 era 10.11.0.47: la IP la dona el DHCP i canvia).

## Exercici 2 · Provisioning

Temps des que el node rep les credencials per BLE fins que té IP (port sèrie):

| Mesura | Temps (ms) |
|---|---|
| 1 | 2232 |
| 2 | 1238 |
| 3 | 778 |
| 4 | 748 |
| 5 | 708 |

- Mitjana 1140,8 ms · desviació estàndard (mostral) 646,8 ms · mediana 778 ms · rang 708–2232 ms.
- Sense la primera mesura: 868 ± 248 ms. La primera és la més lenta (primer cop que el DHCP veia el node).
- Amb el programa final s'han vist també 642 ms i 690 ms (no comptats a les cinc mesures).
- Sempre connecta al primer intent; RSSI entre −57 i −71 dBm.
- Clau incorrecta: intents 1, 2 i 3 de 10 s → `SENSE CREDENCIALS (3 intents fallits)`.
  Les escriptures rebudes durant CONNECTANT s'ignoren (`Ignorades: ara no estic esperant credencials`).
  Avís del driver `E (24092) wifi:sta is connecting, return error`: el driver encara reintentava l'intent anterior.
- Reset després de la fallada → `NVS buida`: la clau incorrecta no s'ha desat.
- Reset amb credencials bones → `NVS: SSID "Lab-Modul" trobat. Em connecto sense passar per BLE` → OPERATIU al primer intent.
- Notificacions d'estat al mòbil: amb l'nRF Connect (iOS) el descriptor 2902 sempre llegeix «Disabled». Pendent de provar amb LightBlue.

## Exercici 3 · Restabliment de fàbrica

- Polsació de més de 5 s en OPERATIU: al tercer segon pantalla vermella «RESET?» amb compte enrere i barra, LED a 10 Hz →
  «ESBORRANT» → reinici → `NVS buida` → SENSE CREDENCIALS (vídeo).
- Després del restabliment el mòbil torna a veure el servei anunciat i el node accepta credencials noves (OPERATIU, POST 201).
- Deixar anar als 4,1 s: `[RESET] Boto deixat anar als 4.1 s: cancel.lat, no passa res`, i continua OPERATIU.
- Mentre el polsador està premut no s'envia cap POST (el POST #6 va sortir 12 s després de l'anterior).

## Exercici 5 · REST

Node-RED, provat amb curl des del PC:
- 201 `{"resultat":"desat"}` · 400 `{"error":"JSON invalid"}` · 422 `Falten camps` · 422 `Temperatura fora de rang` (150).
- Fitxer `/home/pau/dades/telemetria.jsonl` (l'usuari del Raspberry és `pau`, no `pi`). Només s'hi desen les mostres amb 201.

ESP32:
- Un POST cada 10 s en OPERATIU; temps de resposta típic de 35 a 210 ms.
- Sensor sense dada (mal contacte del DHT22) → `"temperatura":null` → 422 → compta com a fallit.
- Desconnexió del Raspberry durant 60 s (`rfkill block wifi; sleep 60; rfkill unblock wifi`), a les 17:19:
  - POST #44 → 201 (`temps_activitat` 569 s, `rebut` 15:19:07Z)
  - POST #45 a #50 → `ERROR -1, connection refused` en 2003 ms cadascun (és el límit de connexió de 2 s), fallits de 7 a 12
  - POST #51 → 201 (`temps_activitat` 639 s, `rebut` 15:20:17Z)
  - El node es manté OPERATIU i es recupera sol al primer intent després de la desconnexió.
  - Al fitxer hi ha un forat de 70 s entre 569 i 639: 6 mostres perdudes, perquè el node no les guarda.

## Exercici 4 · Pendent

- La característica temperatura (`06461B91-9893-4888-8BFF-970F5A6E4274`, lectura i notificació) es veu a l'nRF Connect.
- Falta mesurar les notificacions per minut amb el sensor en repòs i escalfant-se.
