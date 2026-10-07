// P2 - Configuracio del node: exercicis 1 a 5
// Node: M5Stack AtomS3. LED a G7, polsador a G6 (l'altra pota a GND), DHT22 a G8 (com a la P1)
//
// Maquina d'estats (la de l'enunciat):
//   INICI -> (NVS buida) -> SENSE CREDENCIALS -> (credencials per BLE) -> CONNECTANT -> (IP) -> OPERATIU
//   INICI -> (NVS amb credencials) -> CONNECTANT
//   CONNECTANT -> (3 intents fallits) -> SENSE CREDENCIALS
//   OPERATIU   -> (cau la xarxa)       -> CONNECTANT
//   Qualsevol estat -> (polsador premut mes de 5 s) -> esborra la NVS i reinicia (exercici 3)
//
// LED: SENSE CREDENCIALS = 2 Hz, CONNECTANT = 5 Hz, OPERATIU = fix, avis de restabliment = 10 Hz
// La clau de la Wi-Fi no s'imprimeix mai: nomes el SSID i la longitud de la clau.
//
// Ordres pel Serial Monitor (115200 baud, final de linia "New Line"):
//   esborra              esborra les credencials de la NVS i reinicia (per repetir les mesures)
//   servidor 10.11.1.11  canvia la IP del Raspberry i la desa a la NVS (la dona el DHCP i canvia)
//   info                 mostra l'estat, la IP, el servidor i els comptadors

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

// --- UUID de 128 bits generats a l'atzar ---
#define SERVICE_UUID     "9bac5b41-9fb7-47ad-8612-d713991d143c"   // servei de configuracio
#define CRED_UUID        "6a02f0e3-afe4-4321-852d-a75a6d070747"   // caracteristica credencials
#define ESTAT_UUID       "3dbd25f7-7a63-475e-ba15-a39e0c2f6db0"   // caracteristica estat
#define TEMP_UUID        "06461b91-9893-4888-8bff-970f5a6e4274"   // caracteristica temperatura (exercici 4)

#define NOM_BLE   "Node-Pau"
#define PIN_LED   7
#define PIN_BOTO  6
#define PIN_DHT   8

// Exercici 2
const unsigned long TEMPS_INTENT = 10000;   // temps maxim de cada intent de connexio (ms)
const int           MAX_INTENTS  = 3;
// Exercici 3
const unsigned long TEMPS_AVIS   = 3000;    // a partir del tercer segon, avis visible
const unsigned long TEMPS_RESET  = 5000;    // mes de cinc segons: restabliment de fabrica
const unsigned long ANTIREBOTS   = 30;      // ms que el boto ha d'estar estable
// Exercici 4
const unsigned long PERIODE_DHT  = 2000;    // llegim el DHT22 cada dos segons
// Exercici 5
const unsigned long PERIODE_POST = 10000;   // un POST cada deu segons, nomes en OPERATIU
#define SERVIDOR_PER_DEFECTE "10.11.1.11"   // IP del Raspberry el 07/10. Si canvia: ordre "servidor"
const int           PORT_NODERED = 1880;

// --- Estats ---
enum Estat { INICI, SENSE_CREDENCIALS, CONNECTANT, OPERATIU };
Estat estat = INICI;
const char* NOMS_ESTAT[] = { "INICI", "SENSE CREDENCIALS", "CONNECTANT", "OPERATIU" };

// --- BLE ---
BLEServer*         servidor  = nullptr;
BLECharacteristic* charCred  = nullptr;
BLECharacteristic* charEstat = nullptr;
BLECharacteristic* charTemp  = nullptr;
BLE2902*           cccdTemp  = nullptr;   // descriptor 2902: ens diu si el mobil ha activat les notificacions
bool   centralConnectat = false;
bool   centralNou       = false;          // un mobil s'acaba de connectar (encara no li hem enviat res)
bool   credRebudes      = false;
String credText         = "";

// --- Wi-Fi i NVS ---
Preferences nvs;                 // espai de noms "wifi": claus "ssid" i "pass". Espai "rest": clau "ip"
String ssid = "";
String clau = "";
bool   credNoves = false;        // true si venen del mobil (cal desar-les si funcionen)
int    intents = 0;
unsigned long iniciIntent = 0;
unsigned long instantCredencials = 0;   // per cronometrar el temps fins a tenir IP

// --- Sensor (exercici 4) ---
DHT dht(PIN_DHT, DHT22);
float temperatura = NAN;
float humitat     = NAN;
unsigned long ultimaLecturaDht  = 0;
unsigned long ultimaLecturaBona = 0;
int  errorsDht          = 0;
int  ultimaTempEnviada  = 0;     // en decimes de grau
bool hiHaTempEnviada    = false;
int  notificacionsMinut = 0;
unsigned long iniciMinut = 0;

// --- REST (exercici 5) ---
String servidorIp = SERVIDOR_PER_DEFECTE;
unsigned long ultimPost = 0;
int numPost = 0, postOk = 0, postFallits = 0, ultimCodi = 0;

// --- Polsador (exercici 3) ---
bool lecturaAnterior = false;        // ultima lectura (pot ser un rebot)
unsigned long instantCanviBoto = 0;  // quan ha canviat la lectura per ultim cop
bool botoPremut    = false;          // estat estable, ja filtrat
bool botoAlliberat = false;          // s'ha vist el boto deixat anar des de l'arrencada
unsigned long iniciPolsacio = 0;
bool avisReset = false;              // estem mostrant l'avis (entre el segon 3 i el 5)

// --- LED ---
unsigned long ultimCanviLed = 0;
bool ledEnces = false;

// ------------------------------------------------------------------
// Pantalla (128x128). Es guarda el que hi ha escrit per poder-la redibuixar
// despres de l'avis de restabliment
// ------------------------------------------------------------------
#define Y_TITOL    4    // mida 1
#define Y_ESTAT   20    // estat, mida 2
#define Y_DETALL  44    // SSID o IP, mida 1.5
#define Y_TEMPS   62    // intents o temps de configuracio, mida 1.5
#define Y_SENSOR  82    // temperatura i humitat, mida 1.5
#define Y_HTTP   106    // ultim POST, mida 1

String   pEstat  = "INICI";
uint16_t cEstat  = TFT_WHITE;
String   pDetall = "";
String   pTemps  = "";
uint16_t cTemps  = TFT_WHITE;

void pantallaLinia(int y, const char* text, uint16_t color, float mida) {
  M5.Display.fillRect(0, y, 128, (int)(8 * mida) + 4, TFT_BLACK);
  M5.Display.setTextSize(mida);
  M5.Display.setCursor(4, y + 2);
  M5.Display.setTextColor(color, TFT_BLACK);
  M5.Display.print(text);
}

// Arrodoneix a una decima i ho dona en text ("23.4"). Es fa servir igual al mobil, a la pantalla i al JSON
String unDecimal(float v) {
  char txt[12];
  snprintf(txt, sizeof(txt), "%.1f", lroundf(v * 10) / 10.0f);
  return String(txt);
}

void dibuixaSensor() {
  if (avisReset) return;
  String txt = "--.-C --.-%";
  if (!isnan(temperatura)) {
    txt = unDecimal(temperatura) + "C " + unDecimal(humitat) + "%";
  }
  pantallaLinia(Y_SENSOR, txt.c_str(), TFT_CYAN, 1.5);
}

void dibuixaHttp() {
  if (avisReset) return;
  if (estat != OPERATIU) {
    pantallaLinia(Y_HTTP, "", TFT_WHITE, 1);
    return;
  }
  char txt[32];
  uint16_t color = TFT_GREEN;
  if (numPost == 0) {
    snprintf(txt, sizeof(txt), "POST -> %s", servidorIp.c_str());
  } else {
    snprintf(txt, sizeof(txt), "POST %d ok:%d ko:%d", ultimCodi, postOk, postFallits);
    if (ultimCodi != 201) color = TFT_ORANGE;
  }
  pantallaLinia(Y_HTTP, txt, color, 1);
}

void dibuixaPantalla() {
  if (avisReset) return;
  M5.Display.fillScreen(TFT_BLACK);
  pantallaLinia(Y_TITOL, "P2 - " NOM_BLE, TFT_WHITE, 1);
  pantallaLinia(Y_ESTAT, pEstat.c_str(), cEstat, 2);
  pantallaLinia(Y_DETALL, pDetall.c_str(), TFT_WHITE, 1.5);
  pantallaLinia(Y_TEMPS, pTemps.c_str(), cTemps, 1.5);
  dibuixaSensor();
  dibuixaHttp();
}

void posaEstat(const char* text, uint16_t color) {
  pEstat = text;
  cEstat = color;
  if (!avisReset) pantallaLinia(Y_ESTAT, text, color, 2);
}

void posaDetall(const String& text) {
  pDetall = text;
  if (!avisReset) pantallaLinia(Y_DETALL, text.c_str(), TFT_WHITE, 1.5);
}

void posaTemps(const String& text, uint16_t color) {
  pTemps = text;
  cTemps = color;
  if (!avisReset) pantallaLinia(Y_TEMPS, text.c_str(), color, 1.5);
}

// ------------------------------------------------------------------
// Publica un text a la caracteristica 'estat' i el notifica si hi ha mobil
// ------------------------------------------------------------------
void notificaEstat(const String& text) {
  charEstat->setValue(text.c_str());
  if (centralConnectat) {
    charEstat->notify();
  }
  Serial.printf("ESTAT -> %s\n", text.c_str());
}

// ------------------------------------------------------------------
// Wi-Fi
// ------------------------------------------------------------------
void comencaIntent() {
  intents++;
  iniciIntent = millis();
  Serial.printf("Intent %d de %d: SSID \"%s\", clau de %d caracters\n",
                intents, MAX_INTENTS, ssid.c_str(), clau.length());
  posaTemps(String("intent ") + intents + "/" + MAX_INTENTS, TFT_WHITE);
  WiFi.disconnect();
  delay(50);
  WiFi.begin(ssid.c_str(), clau.c_str());
}

// ------------------------------------------------------------------
// Canvi d'estat: aqui es fa tot el que toca en ENTRAR a cada estat
// ------------------------------------------------------------------
void canviaEstat(Estat nou, const String& detall) {
  estat = nou;

  switch (nou) {
    case SENSE_CREDENCIALS: {
      WiFi.disconnect();
      String text = "SENSE CREDENCIALS";
      if (detall.length() > 0) {
        text += " ";
        text += detall;
      }
      notificaEstat(text);
      posaEstat("SENSE CRED", TFT_RED);
      posaDetall("Esperant BLE");
      posaTemps("", TFT_WHITE);
      if (!centralConnectat) {
        BLEDevice::startAdvertising();   // tornem a ser visibles per BLE
      }
      break;
    }

    case CONNECTANT:
      intents = 0;
      notificaEstat("CONNECTANT");
      posaEstat("CONNECTANT", TFT_YELLOW);
      posaDetall(ssid);
      comencaIntent();
      break;

    case OPERATIU: {
      BLEDevice::getAdvertising()->stop();   // ja tenim xarxa: deixem d'anunciar-nos
      String ip = WiFi.localIP().toString();
      notificaEstat(String("OPERATIU ") + ip);
      posaEstat("OPERATIU", TFT_GREEN);
      posaDetall(ip);
      Serial.printf("Connectat. IP %s, RSSI %d dBm\n", ip.c_str(), WiFi.RSSI());

      if (credNoves) {
        // Nomes desem quan sabem que les credencials funcionen
        nvs.begin("wifi", false);
        nvs.putString("ssid", ssid);
        nvs.putString("pass", clau);
        nvs.end();
        credNoves = false;
        Serial.println("Credencials desades a la NVS");

        unsigned long t = millis() - instantCredencials;
        Serial.printf("TEMPS des de les credencials fins a tenir IP: %lu ms\n", t);
        char txt[24];
        snprintf(txt, sizeof(txt), "t = %.2f s", t / 1000.0);
        posaTemps(txt, TFT_CYAN);
      } else {
        posaTemps("", TFT_WHITE);
      }

      // Exercici 5: el primer POST surt al cap de 2 s i despues cada 10 s
      ultimPost = millis() - PERIODE_POST + 2000;
      Serial.printf("[REST] Envio a http://%s:%d/api/telemetria cada %lu s\n",
                    servidorIp.c_str(), PORT_NODERED, PERIODE_POST / 1000);
      break;
    }

    default:
      break;
  }
  dibuixaHttp();   // la linia del POST nomes es veu en OPERATIU
}

// ------------------------------------------------------------------
// Valida el JSON rebut per BLE. Ha de ser {"ssid":"...","pass":"..."}
// ------------------------------------------------------------------
bool validaCredencials(const String& text) {
  JsonDocument doc;
  if (deserializeJson(doc, text)) {
    Serial.println("JSON invalid: no es pot llegir");
    return false;
  }
  if (!doc["ssid"].is<const char*>() || !doc["pass"].is<const char*>()) {
    Serial.println("JSON invalid: falten els camps ssid o pass");
    return false;
  }
  String s = doc["ssid"].as<String>();
  String p = doc["pass"].as<String>();
  if (s.length() == 0 || s.length() > 32 || p.length() > 63) {
    Serial.println("JSON invalid: SSID buit o massa llarg, o clau massa llarga");
    return false;
  }
  ssid = s;
  clau = p;
  return true;
}

// ------------------------------------------------------------------
// Exercici 4: lectura del DHT22 i notificacio de la temperatura
// ------------------------------------------------------------------
void publicaTemperatura() {
  int decimes = (int)lroundf(temperatura * 10);
  String txt = unDecimal(temperatura);
  charTemp->setValue(txt);                 // una lectura des del mobil sempre dona l'ultim valor

  if (centralNou) {                        // mobil nou: encara no li hem enviat cap valor
    centralNou = false;
    hiHaTempEnviada = false;
  }
  // Nomes notifiquem si hi ha un mobil connectat que ho ha demanat (2902 activat)...
  if (!centralConnectat || !cccdTemp->getNotifications()) return;
  // ... i si el valor ha canviat com a minim una decima respecte de l'ultim enviat
  if (hiHaTempEnviada && abs(decimes - ultimaTempEnviada) < 1) return;

  charTemp->notify();
  ultimaTempEnviada = decimes;
  hiHaTempEnviada = true;
  notificacionsMinut++;
  Serial.printf("[BLE] temperatura -> %s (notificada)\n", txt.c_str());
}

void llegeixSensor() {
  float t = dht.readTemperature(false, true);   // true = lectura nova de veritat
  float h = dht.readHumidity();                 // fa servir la mateixa lectura
  if (isnan(t) || isnan(h)) {
    errorsDht++;
    Serial.printf("[DHT] Lectura fallida (%d en total)\n", errorsDht);
    if (millis() - ultimaLecturaBona > 3 * PERIODE_DHT) {   // fa massa que no en tenim cap de bona
      temperatura = NAN;
      humitat = NAN;
      dibuixaSensor();
    }
    return;
  }
  temperatura = t;
  humitat = h;
  ultimaLecturaBona = millis();
  dibuixaSensor();
  publicaTemperatura();
}

// ------------------------------------------------------------------
// Exercici 5: POST de telemetria al Node-RED del Raspberry
// ------------------------------------------------------------------
void enviaTelemetria() {
  numPost++;
  JsonDocument doc;
  doc["id"] = NOM_BLE;
  if (isnan(temperatura)) {
    doc["temperatura"] = nullptr;          // sense lectura: el Node-RED respondra 422
    doc["humitat"] = nullptr;
  } else {
    doc["temperatura"] = serialized(unDecimal(temperatura));
    doc["humitat"] = serialized(unDecimal(humitat));
  }
  doc["rssi"] = WiFi.RSSI();
  doc["temps_activitat"] = millis() / 1000;   // segons des de l'arrencada
  String cos;
  serializeJson(doc, cos);

  String url = String("http://") + servidorIp + ":" + PORT_NODERED + "/api/telemetria";
  HTTPClient http;
  http.setConnectTimeout(2000);   // si el Raspberry no hi es, no ens quedem penjats gaire estona
  http.setTimeout(3000);
  unsigned long t0 = millis();
  int codi = HTTPC_ERROR_CONNECTION_REFUSED;
  if (http.begin(url)) {
    http.addHeader("Content-Type", "application/json");
    codi = http.POST(cos);
    http.end();
  }
  unsigned long durada = millis() - t0;

  ultimCodi = codi;
  if (codi == 201) {
    postOk++;
  } else {
    postFallits++;
  }
  if (codi > 0) {
    Serial.printf("[REST] POST #%d -> %d (%lu ms) | ok %d, fallits %d | %s\n",
                  numPost, codi, durada, postOk, postFallits, cos.c_str());
  } else {
    Serial.printf("[REST] POST #%d -> ERROR %d, %s (%lu ms) | ok %d, fallits %d\n",
                  numPost, codi, HTTPClient::errorToString(codi).c_str(), durada, postOk, postFallits);
  }
  dibuixaHttp();
}

// ------------------------------------------------------------------
// Exercici 3: restabliment de fabrica amb una polsacio llarga
// ------------------------------------------------------------------
void restabliment() {
  Serial.println("[RESET] 5 s: restabliment de fabrica. Esborro l'espai de noms \"wifi\" de la NVS i reinicio");
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_RED, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(4, 50);
  M5.Display.print("ESBORRANT");
  digitalWrite(PIN_LED, LOW);
  notificaEstat("RESTABLINT");     // si hi ha un mobil connectat, ho veu
  nvs.begin("wifi", false);
  nvs.clear();
  nvs.end();
  delay(500);
  ESP.restart();
}

void mostraAvisReset(unsigned long durada) {
  static unsigned long ultimDibuix = 0;
  if (!avisReset) {
    avisReset = true;
    ultimDibuix = 0;
    M5.Display.fillScreen(TFT_RED);
    M5.Display.setTextColor(TFT_WHITE, TFT_RED);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(4, 8);
    M5.Display.print("RESET?");
    M5.Display.setTextSize(1);
    M5.Display.setCursor(4, 100);
    M5.Display.print("Deixa anar per");
    M5.Display.setCursor(4, 112);
    M5.Display.print("cancel.lar");
    Serial.println("[RESET] Boto premut 3 s. Si arriba a 5 s s'esborra la NVS");
  }
  if (millis() - ultimDibuix < 100) return;   // refresquem el compte enrere 10 cops per segon
  ultimDibuix = millis();

  char txt[12];
  snprintf(txt, sizeof(txt), "%.1f s", (TEMPS_RESET - durada) / 1000.0);
  M5.Display.fillRect(0, 34, 128, 32, TFT_RED);
  M5.Display.setTextColor(TFT_WHITE, TFT_RED);
  M5.Display.setTextSize(3);
  M5.Display.setCursor(4, 38);
  M5.Display.print(txt);
  int ample = (int)(120.0 * (durada - TEMPS_AVIS) / (TEMPS_RESET - TEMPS_AVIS));
  M5.Display.fillRect(4, 76, 120, 12, TFT_BLACK);   // fons de la barra
  M5.Display.fillRect(4, 76, ample, 12, TFT_WHITE);
}

void gestionaBoto() {
  bool lectura = (digitalRead(PIN_BOTO) == LOW);      // pull-up intern: premut = LOW
  if (lectura != lecturaAnterior) {                    // ha canviat: pot ser un rebot
    lecturaAnterior = lectura;
    instantCanviBoto = millis();
  }
  if (millis() - instantCanviBoto < ANTIREBOTS) return;   // encara no es estable

  if (!lectura) {                                      // --- boto deixat anar ---
    botoAlliberat = true;
    if (botoPremut) {
      botoPremut = false;
      float segons = (instantCanviBoto - iniciPolsacio) / 1000.0;
      if (avisReset) {
        avisReset = false;
        Serial.printf("[RESET] Boto deixat anar als %.1f s: cancel.lat, no passa res\n", segons);
        dibuixaPantalla();                             // tornem a la pantalla normal
      } else {
        Serial.printf("[BOTO] Polsacio de %.1f s: no fa res\n", segons);
      }
    }
    return;
  }

  // --- boto premut (i estable) ---
  if (!botoAlliberat) return;          // ja estava premut en arrencar: no compta
  if (!botoPremut) {
    botoPremut = true;
    iniciPolsacio = instantCanviBoto;  // el moment en que s'ha premut de veritat
  }
  unsigned long durada = millis() - iniciPolsacio;
  if (durada >= TEMPS_RESET) {
    restabliment();
  } else if (durada >= TEMPS_AVIS) {
    mostraAvisReset(durada);
  }
}

// ------------------------------------------------------------------
// Ordres pel port serie
// ------------------------------------------------------------------
void mostraInfo() {
  Serial.printf("Estat: %s | IP: %s | RSSI: %d dBm | mobil connectat: %s\n",
                NOMS_ESTAT[estat], WiFi.localIP().toString().c_str(), WiFi.RSSI(),
                centralConnectat ? "si" : "no");
  Serial.printf("Servidor: http://%s:%d/api/telemetria | POST: %d (ok %d, fallits %d, ultim codi %d)\n",
                servidorIp.c_str(), PORT_NODERED, numPost, postOk, postFallits, ultimCodi);
  if (isnan(temperatura)) {
    Serial.printf("DHT22: sense lectura | lectures fallides: %d\n", errorsDht);
  } else {
    Serial.printf("DHT22: %s C, %s %% | lectures fallides: %d\n",
                  unDecimal(temperatura).c_str(), unDecimal(humitat).c_str(), errorsDht);
  }
}

void llegeixOrdre() {
  String ordre = Serial.readStringUntil('\n');
  ordre.trim();
  if (ordre == "esborra") {
    nvs.begin("wifi", false);
    nvs.clear();
    nvs.end();
    Serial.println("NVS esborrada. Reinicio...");
    delay(200);
    ESP.restart();
  } else if (ordre.startsWith("servidor")) {
    String ip = ordre.substring(8);
    ip.trim();
    IPAddress prova;
    if (ip.length() == 0) {
      Serial.printf("Servidor actual: %s\n", servidorIp.c_str());
    } else if (!prova.fromString(ip)) {
      Serial.println("IP no valida. Exemple: servidor 10.11.1.11");
    } else {
      servidorIp = ip;
      nvs.begin("rest", false);
      nvs.putString("ip", servidorIp);
      nvs.end();
      Serial.printf("Servidor canviat a %s (desat a la NVS)\n", servidorIp.c_str());
      dibuixaHttp();
    }
  } else if (ordre == "info") {
    mostraInfo();
  } else if (ordre.length() > 0) {
    Serial.println("Ordres: esborra | servidor <ip> | info");
  }
}

// ------------------------------------------------------------------
// Callbacks BLE
// ------------------------------------------------------------------
class CallbacksServidor : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override {
    centralConnectat = true;
    centralNou = true;
    Serial.println("Mobil connectat");
  }
  void onDisconnect(BLEServer* s) override {
    centralConnectat = false;
    Serial.println("Mobil desconnectat");
    if (estat == SENSE_CREDENCIALS) {
      BLEDevice::startAdvertising();   // nomes ens tornem a anunciar si encara cal configurar-nos
    }
  }
};

class CallbacksCredencials : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    credText = c->getValue();
    credRebudes = true;   // es processa al loop
  }
};

// El mobil escriu el descriptor 2902 quan activa o desactiva les notificacions.
// Ho escrivim al port serie per saber si ho ha fet de veritat.
class CallbacksCccd : public BLEDescriptorCallbacks {
 public:
  explicit CallbacksCccd(const char* nom) : nom(nom) {}
  void onWrite(BLEDescriptor* d) override {
    bool actives = ((BLE2902*)d)->getNotifications();
    Serial.printf("[BLE] El mobil %s les notificacions de '%s'\n",
                  actives ? "ha activat" : "ha desactivat", nom);
    if (actives && strcmp(nom, "temperatura") == 0) {
      centralNou = true;   // li enviem el valor actual a la propera lectura, encara que no hagi canviat
    }
  }
 private:
  const char* nom;
};

// Descriptor 2901: el nom de la caracteristica, perque es vegi a l'nRF Connect
void posaNom(BLECharacteristic* c, const char* nom) {
  BLEDescriptor* d = new BLEDescriptor(BLEUUID((uint16_t)0x2901));
  d->setValue(nom);
  c->addDescriptor(d);
}

// ------------------------------------------------------------------
void setup() {
  M5.begin();
  Serial.begin(115200);
  Serial.setTimeout(100);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BOTO, INPUT_PULLUP);   // l'altra pota del polsador a GND
  dht.begin();

  M5.Display.setTextWrap(false);
  dibuixaPantalla();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);   // les reconnexions les porta la maquina d'estats

  // --- Servidor GATT (exercicis 1 i 4) ---
  BLEDevice::init(NOM_BLE);
  servidor = BLEDevice::createServer();
  servidor->setCallbacks(new CallbacksServidor());
  BLEService* servei = servidor->createService(BLEUUID(SERVICE_UUID), 20);   // 20 handles: 3 caracteristiques amb descriptors

  charCred = servei->createCharacteristic(CRED_UUID, BLECharacteristic::PROPERTY_WRITE);
  charCred->setCallbacks(new CallbacksCredencials());
  posaNom(charCred, "credencials");

  charEstat = servei->createCharacteristic(
      ESTAT_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  BLE2902* cccdEstat = new BLE2902();
  cccdEstat->setCallbacks(new CallbacksCccd("estat"));
  charEstat->addDescriptor(cccdEstat);
  posaNom(charEstat, "estat");

  charTemp = servei->createCharacteristic(
      TEMP_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  cccdTemp = new BLE2902();
  cccdTemp->setCallbacks(new CallbacksCccd("temperatura"));
  charTemp->addDescriptor(cccdTemp);
  posaNom(charTemp, "temperatura");
  charTemp->setValue("--.-");
  servei->start();

  BLEAdvertising* anunci = BLEDevice::getAdvertising();
  anunci->addServiceUUID(SERVICE_UUID);
  anunci->setScanResponse(true);

  // --- Servidor REST (exercici 5): la IP es pot canviar amb l'ordre "servidor" ---
  nvs.begin("rest", true);
  servidorIp = nvs.getString("ip", SERVIDOR_PER_DEFECTE);
  nvs.end();
  Serial.printf("Servidor REST: http://%s:%d/api/telemetria\n", servidorIp.c_str(), PORT_NODERED);

  // --- INICI: llegim la NVS ---
  notificaEstat("INICI");
  posaEstat("INICI", TFT_WHITE);
  nvs.begin("wifi", true);   // true = nomes lectura
  ssid = nvs.getString("ssid", "");
  clau = nvs.getString("pass", "");
  nvs.end();

  if (ssid.length() == 0) {
    Serial.println("NVS buida");
    canviaEstat(SENSE_CREDENCIALS, "");
  } else {
    Serial.printf("NVS: SSID \"%s\" trobat. Em connecto sense passar per BLE\n", ssid.c_str());
    credNoves = false;
    canviaEstat(CONNECTANT, "");
  }
  iniciMinut = millis();
}

// ------------------------------------------------------------------
void loop() {
  M5.update();

  // --- 0. Polsador: restabliment de fabrica (exercici 3) ---
  gestionaBoto();

  // --- 1. Credencials rebudes per BLE (exercici 2) ---
  if (credRebudes) {
    credRebudes = false;
    Serial.printf("Rebut a credencials: %d bytes\n", credText.length());
    if (estat != SENSE_CREDENCIALS) {
      Serial.println("Ignorades: ara no estic esperant credencials");
    } else if (validaCredencials(credText)) {
      instantCredencials = millis();   // comencem a cronometrar
      credNoves = true;
      canviaEstat(CONNECTANT, "");     // es notifica CONNECTANT immediatament
    } else {
      notificaEstat("SENSE CREDENCIALS (JSON invalid)");
    }
    credText = "";   // no guardem el text en memoria mes temps del necessari
  }

  // --- 2. Que fa cada estat ---
  switch (estat) {
    case CONNECTANT:
      if (WiFi.status() == WL_CONNECTED) {
        canviaEstat(OPERATIU, "");
      } else if (millis() - iniciIntent > TEMPS_INTENT) {
        Serial.printf("Intent %d fallit (temps esgotat)\n", intents);
        if (intents >= MAX_INTENTS) {
          credNoves = false;
          canviaEstat(SENSE_CREDENCIALS, "(3 intents fallits)");
        } else {
          comencaIntent();
        }
      }
      break;

    case OPERATIU:
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("S'ha perdut la Wi-Fi");
        canviaEstat(CONNECTANT, "");
      } else if (!botoPremut && millis() - ultimPost >= PERIODE_POST) {
        ultimPost = millis();          // (si el boto esta premut, esperem: el POST pot bloquejar)
        enviaTelemetria();             // exercici 5
      }
      break;

    default:
      break;
  }

  // --- 3. Sensor cada 2 s (exercici 4) ---
  if (millis() - ultimaLecturaDht >= PERIODE_DHT) {
    ultimaLecturaDht = millis();
    llegeixSensor();
  }
  if (millis() - iniciMinut >= 60000) {
    if (centralConnectat) {
      Serial.printf("[BLE] Notificacions de temperatura l'ultim minut: %d\n", notificacionsMinut);
    }
    notificacionsMinut = 0;
    iniciMinut = millis();
  }

  // --- 4. LED segons l'estat ---
  unsigned long periode = 0;                         // 0 = fix
  if (estat == SENSE_CREDENCIALS) periode = 250;     // 2 Hz
  if (estat == CONNECTANT)        periode = 100;     // 5 Hz
  if (avisReset)                  periode = 50;      // 10 Hz: s'esborrara la NVS
  if (periode == 0) {
    digitalWrite(PIN_LED, estat == OPERATIU ? HIGH : LOW);
  } else if (millis() - ultimCanviLed >= periode) {
    ultimCanviLed = millis();
    ledEnces = !ledEnces;
    digitalWrite(PIN_LED, ledEnces);
  }

  // --- 5. Ordres pel port serie: esborra | servidor <ip> | info ---
  if (Serial.available()) {
    llegeixOrdre();
  }

  delay(10);
}
