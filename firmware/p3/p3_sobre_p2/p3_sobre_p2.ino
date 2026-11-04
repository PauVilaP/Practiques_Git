// =====================================================================
// P3 sobre P2 - Sistemes Encastats - Pau Vila
// Node: M5Stack AtomS3. LED a G7, DHT22 a G8. Boto: el de l'AtomS3 (la pantalla, G41)
// =====================================================================
//
// Aquest sketch es el de la P2 amb la P3 posada a sobre:
//   1. El node arrenca com a la P2: si no te Wi-Fi desada, espera les credencials per BLE;
//      si en te, s'hi connecta. Maquina d'estats: INICI, SENSE CREDENCIALS, CONNECTANT, OPERATIU.
//   2. Quan la P2 arriba a OPERATIU (ja te IP), la P3 es connecta al broker MQTT i fa la seva
//      feina: telemetria, periode configurable, aturar/reprendre i alarmes.
//
// Pestanyes (cada pestanya de l'Arduino IDE es un fitxer d'aquesta carpeta):
//   p3_sobre_p2.ino      aquesta: llibreries, el que comparteixen la P2 i la P3, setup() i loop()
//   P2_configuracio.ino  el codi de la P2: BLE, Wi-Fi, boto, LED, sensor, pantalla i REST
//   P3_mqtt.ino          el codi de la P3: broker, temes, telemetria, ordres, configuracio i alarmes
//
// Marques als comentaris:
//   [P3]  a la pestanya P2: linia afegida o canviada per poder-hi posar la P3 a sobre
//   [P2]  a la pestanya P3: linia treta o canviada perque ara aixo ja ho fa la P2
//   [BOTO] a la pestanya P2: el polsador extern de G6 s'ha canviat pel boto de l'AtomS3
//   La resta de linies son les mateixes que als programes originals de la P2 i de la P3.
//
// Ordres pel Serial Monitor (115200 baud, final de linia "New Line"):
//   esborra         (P2) esborra les credencials de la Wi-Fi de la NVS i reinicia
//   info            (P2 i P3) estat, IP, comptadors, broker, periode, llindars...
//   servidor <ip>   (P2) IP del servidor REST (el POST nomes surt si REST_ACTIU = true)
//   broker <ip>     (P3) IP del broker MQTT (es desa a la NVS)
//
// L'Arduino IDE ajunta les pestanyes en un sol fitxer: primer aquesta i despres les altres per
// ordre alfabetic (P2_configuracio i P3_mqtt). Per aixo la P3 pot fer servir les variables de
// la P2 (estat, temperatura, nvs...), pero la P2 nomes crida funcions de la P3 (les de sota).
// =====================================================================

// ---- Llibreries de la P2 ----
#include <M5Unified.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <DHT.h>

// ---- Llibreries que afegeix la P3 ----
#include <PubSubClient.h>
#include <time.h>

// ---- Estats de la maquina d'estats de la P2 ----
// Es aqui i no a la pestanya P2 per com treballa l'Arduino IDE: just abans del setup() d'aquesta
// pestanya hi afegeix sol el prototip de cada funcio del sketch, i el de canviaEstat(Estat nou, ...)
// necessita que Estat ja existeixi. A mes, la P3 el fa servir per saber quan el node es OPERATIU.
enum Estat { INICI, SENSE_CREDENCIALS, CONNECTANT, OPERATIU };

// ---- Com es parlen les pestanyes ----
// Cada pestanya te el seu "setup" i el seu "loop", que es criden des d'aqui:
void p2Setup();   void p2Loop();     // a P2_configuracio.ino
void p3Setup();   void p3Loop();     // a P3_mqtt.ino

// Funcions de la P3 que crida la P2 (son a P3_mqtt.ino). Son els unics punts on la P2 avisa la P3:
void     p3EntraOperatiu();              // la P2 acaba d'entrar a OPERATIU: ja hi ha xarxa
void     p3NovaLectura();                // la P2 acaba de llegir be el DHT22: comprovar les alarmes
void     p3DibuixaPantalla();            // la P2 redibuixa la pantalla: afegir-hi les linies de la P3
uint16_t p3ColorFons();                  // color de fons de la pantalla: vermell si hi ha alarma
uint16_t p3ColorTemp();                  // color de la temperatura: groc si esta en alarma
uint16_t p3ColorHum();                   // color de la humitat: groc si esta en alarma
bool     p3Ordre(const String& linia);   // ordre del port serie que la P2 no coneix (broker)
void     p3Info();                       // la part de la P3 de l'ordre "info"


// =====================================================================
void setup() {
  // Comu a la P2 i a la P3 (abans hi era a tots dos setup)
  M5.begin();
  Serial.begin(115200);
  Serial.setTimeout(100);
  M5.Display.setTextWrap(false);

  p2Setup();   // P2: LED, boto, DHT22, servidor BLE, NVS i maquina d'estats (comenca la Wi-Fi)
  p3Setup();   // P3: identificador de client, adreca del broker i hora per NTP
}

void loop() {
  M5.update();
  p2Loop();    // P2: boto, credencials BLE, Wi-Fi, sensor cada 2 s, LED i ordres pel port serie
  p3Loop();    // P3: connexio al broker, missatges rebuts i telemetria cada "periode" segons
  delay(10);
}
