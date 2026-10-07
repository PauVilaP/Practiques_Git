// P2 - Exercicis 1 i 2 - Servidor GATT propi i provisioning de la Wi-Fi
// Node: M5Stack AtomS3. LED a G7, polsador a G6, DHT22 a G8 (com a la P1)
//
// Maquina d'estats (la de l'enunciat):
//   INICI -> (NVS buida) -> SENSE CREDENCIALS -> (credencials per BLE) -> CONNECTANT -> (IP) -> OPERATIU
//   CONNECTANT -> (3 intents fallits) -> SENSE CREDENCIALS
//   OPERATIU   -> (cau la xarxa)       -> CONNECTANT
//
// LED: SENSE CREDENCIALS = 2 Hz, CONNECTANT = 5 Hz, OPERATIU = fix
// La clau de la Wi-Fi no s'imprimeix mai: nomes el SSID i la longitud de la clau.

#include <M5Unified.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// --- UUID de 128 bits generats a l'atzar ---
#define SERVICE_UUID     "9bac5b41-9fb7-47ad-8612-d713991d143c"   // servei de configuracio
#define CRED_UUID        "6a02f0e3-afe4-4321-852d-a75a6d070747"   // caracteristica credencials
#define ESTAT_UUID       "3dbd25f7-7a63-475e-ba15-a39e0c2f6db0"   // caracteristica estat

#define NOM_BLE  "Node-Pau"
#define PIN_LED  7

const unsigned long TEMPS_INTENT = 10000;   // temps maxim de cada intent de connexio (ms)
const int           MAX_INTENTS  = 3;

// --- Estats ---
enum Estat { INICI, SENSE_CREDENCIALS, CONNECTANT, OPERATIU };
Estat estat = INICI;

// --- BLE ---
BLEServer*         servidor  = nullptr;
BLECharacteristic* charCred  = nullptr;
BLECharacteristic* charEstat = nullptr;
bool   centralConnectat = false;
bool   credRebudes      = false;
String credText         = "";

// --- Wi-Fi i NVS ---
Preferences nvs;                 // espai de noms "wifi", claus "ssid" i "pass"
String ssid = "";
String clau = "";
bool   credNoves = false;        // true si venen del mobil (cal desar-les si funcionen)
int    intents = 0;
unsigned long iniciIntent = 0;
unsigned long instantCredencials = 0;   // per cronometrar el temps fins a tenir IP

// --- LED ---
unsigned long ultimCanviLed = 0;
bool ledEnces = false;

// ------------------------------------------------------------------
// Pantalla (128x128): titol, estat en gran, i a sota SSID o IP i el temps
// ------------------------------------------------------------------
#define Y_ESTAT   24    // estat, mida 2
#define Y_DETALL  52    // SSID o IP, mida 1.5
#define Y_TEMPS   72    // temps de configuracio, mida 1.5

void pantallaLinia(int y, const char* text, uint16_t color, float mida) {
  M5.Display.fillRect(0, y, 128, (int)(8 * mida) + 4, TFT_BLACK);
  M5.Display.setTextSize(mida);
  M5.Display.setCursor(4, y + 2);
  M5.Display.setTextColor(color, TFT_BLACK);
  M5.Display.print(text);
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
  char txt[16];
  snprintf(txt, sizeof(txt), "intent %d/%d", intents, MAX_INTENTS);
  pantallaLinia(Y_TEMPS, txt, TFT_WHITE, 1.5);
  WiFi.disconnect();
  delay(50);
  WiFi.begin(ssid.c_str(), clau.c_str());
}

// ------------------------------------------------------------------
// Canvi d'estat: aqui es fa tot el que toca en ENTRAR a cada estat
// ------------------------------------------------------------------
void canviaEstat(Estat nou, const String& detall = "") {
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
      pantallaLinia(Y_ESTAT, "SENSE CRED", TFT_RED, 2);
      pantallaLinia(Y_DETALL, "Esperant BLE", TFT_WHITE, 1.5);
      pantallaLinia(Y_TEMPS, "", TFT_WHITE, 1.5);
      if (!centralConnectat) {
        BLEDevice::startAdvertising();   // tornem a ser visibles per BLE
      }
      break;
    }

    case CONNECTANT:
      intents = 0;
      notificaEstat("CONNECTANT");
      pantallaLinia(Y_ESTAT, "CONNECTANT", TFT_YELLOW, 2);
      pantallaLinia(Y_DETALL, ssid.c_str(), TFT_WHITE, 1.5);
      comencaIntent();
      break;

    case OPERATIU: {
      BLEDevice::getAdvertising()->stop();   // ja tenim xarxa: deixem d'anunciar-nos
      String ip = WiFi.localIP().toString();
      notificaEstat(String("OPERATIU ") + ip);
      pantallaLinia(Y_ESTAT, "OPERATIU", TFT_GREEN, 2);
      pantallaLinia(Y_DETALL, ip.c_str(), TFT_WHITE, 1.5);
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
        pantallaLinia(Y_TEMPS, txt, TFT_CYAN, 1.5);
      } else {
        pantallaLinia(Y_TEMPS, "", TFT_WHITE, 1.5);
      }
      break;
    }

    default:
      break;
  }
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
// Callbacks BLE
// ------------------------------------------------------------------
class CallbacksServidor : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override {
    centralConnectat = true;
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
    credText = c->getValue().c_str();
    credRebudes = true;   // es processa al loop
  }
};

// ------------------------------------------------------------------
void setup() {
  M5.begin();
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextWrap(false);
  pantallaLinia(4, "P2 - " NOM_BLE, TFT_WHITE, 1);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);   // les reconnexions les porta la maquina d'estats

  // --- Servidor GATT (exercici 1) ---
  BLEDevice::init(NOM_BLE);
  servidor = BLEDevice::createServer();
  servidor->setCallbacks(new CallbacksServidor());
  BLEService* servei = servidor->createService(SERVICE_UUID);

  charCred = servei->createCharacteristic(CRED_UUID, BLECharacteristic::PROPERTY_WRITE);
  charCred->setCallbacks(new CallbacksCredencials());

  charEstat = servei->createCharacteristic(
      ESTAT_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  charEstat->addDescriptor(new BLE2902());
  servei->start();

  BLEAdvertising* anunci = BLEDevice::getAdvertising();
  anunci->addServiceUUID(SERVICE_UUID);
  anunci->setScanResponse(true);

  // --- INICI: llegim la NVS ---
  notificaEstat("INICI");
  pantallaLinia(Y_ESTAT, "INICI", TFT_WHITE, 2);
  nvs.begin("wifi", true);   // true = nomes lectura
  ssid = nvs.getString("ssid", "");
  clau = nvs.getString("pass", "");
  nvs.end();

  if (ssid.length() == 0) {
    Serial.println("NVS buida");
    canviaEstat(SENSE_CREDENCIALS);
  } else {
    Serial.printf("NVS: SSID \"%s\" trobat. Em connecto sense passar per BLE\n", ssid.c_str());
    credNoves = false;
    canviaEstat(CONNECTANT);
  }
}

// ------------------------------------------------------------------
void loop() {
  M5.update();

  // --- 1. Credencials rebudes per BLE ---
  if (credRebudes) {
    credRebudes = false;
    Serial.printf("Rebut a credencials: %d bytes\n", credText.length());
    if (estat != SENSE_CREDENCIALS) {
      Serial.println("Ignorades: ara no estic esperant credencials");
    } else if (validaCredencials(credText)) {
      instantCredencials = millis();   // comencem a cronometrar
      credNoves = true;
      canviaEstat(CONNECTANT);         // es notifica CONNECTANT immediatament
    } else {
      notificaEstat("SENSE CREDENCIALS (JSON invalid)");
    }
    credText = "";   // no guardem el text en memoria mes temps del necessari
  }

  // --- 2. Que fa cada estat ---
  switch (estat) {
    case CONNECTANT:
      if (WiFi.status() == WL_CONNECTED) {
        canviaEstat(OPERATIU);
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
        canviaEstat(CONNECTANT);
      }
      break;

    default:
      break;
  }

  // --- 3. LED segons l'estat ---
  unsigned long periode = 0;                   // 0 = fix
  if (estat == SENSE_CREDENCIALS) periode = 250;   // 2 Hz
  if (estat == CONNECTANT)        periode = 100;   // 5 Hz
  if (periode == 0) {
    digitalWrite(PIN_LED, estat == OPERATIU ? HIGH : LOW);
  } else if (millis() - ultimCanviLed >= periode) {
    ultimCanviLed = millis();
    ledEnces = !ledEnces;
    digitalWrite(PIN_LED, ledEnces);
  }

  // --- 4. Ordre per port serie per a proves: escriu "esborra" ---
  if (Serial.available()) {
    String ordre = Serial.readStringUntil('\n');
    ordre.trim();
    if (ordre == "esborra") {
      nvs.begin("wifi", false);
      nvs.clear();
      nvs.end();
      Serial.println("NVS esborrada. Reinicio...");
      delay(200);
      ESP.restart();
    }
  }

  delay(10);
}
