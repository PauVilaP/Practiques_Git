// P2 - Exercici 1 - Servidor GATT propi
// Node: M5Stack AtomS3. LED a G7, polsador a G6, DHT22 a G8 (com a la P1)
//
// Servei de configuracio amb dues caracteristiques:
//   - credencials: escriptura. Rep un JSON amb el SSID i la clau de la Wi-Fi.
//   - estat:       lectura i notificacio. Publica l'estat del node en text pla.

#include <M5Unified.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// --- UUID de 128 bits generats a l'atzar ---
#define SERVICE_UUID     "9bac5b41-9fb7-47ad-8612-d713991d143c"   // servei de configuracio
#define CRED_UUID        "6a02f0e3-afe4-4321-852d-a75a6d070747"   // caracteristica credencials
#define ESTAT_UUID       "3dbd25f7-7a63-475e-ba15-a39e0c2f6db0"   // caracteristica estat

#define NOM_BLE  "Node-Pau"   // nom amb que s'anuncia (GAP)
#define PIN_LED  7

BLEServer*         servidor = nullptr;
BLECharacteristic* charCred  = nullptr;
BLECharacteristic* charEstat = nullptr;

bool   centralConnectat = false;
bool   credRebudes      = false;   // el callback nomes posa la bandera; la feina es fa al loop
String credText         = "";

// Canvia l'estat: el desa a la caracteristica, el notifica i el mostra a la pantalla
void publicaEstat(const char* text) {
  charEstat->setValue(text);
  if (centralConnectat) {
    charEstat->notify();
  }
  Serial.printf("ESTAT -> %s\n", text);

  M5.Display.fillRect(0, 40, 128, 40, BLACK);
  M5.Display.setCursor(4, 45);
  M5.Display.setTextColor(YELLOW, BLACK);
  M5.Display.print(text);
}

// --- Que passa quan el mobil es connecta o es desconnecta ---
class CallbacksServidor : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override {
    centralConnectat = true;
    Serial.println("Mobil connectat");
  }
  void onDisconnect(BLEServer* s) override {
    centralConnectat = false;
    Serial.println("Mobil desconnectat, torno a anunciar-me");
    BLEDevice::startAdvertising();   // sense aixo, despres de desconnectar ja no es veuria
  }
};

// --- Que passa quan el mobil escriu a 'credencials' ---
class CallbacksCredencials : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    credText = c->getValue().c_str();
    credRebudes = true;   // ho processem al loop, aqui dins no s'ha de fer feina llarga
  }
};

void setup() {
  M5.begin();
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);

  M5.Display.fillScreen(BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.setCursor(4, 4);
  M5.Display.print("P2 - ");
  M5.Display.print(NOM_BLE);

  // 1. Engegar el BLE amb el nostre nom
  BLEDevice::init(NOM_BLE);
  servidor = BLEDevice::createServer();
  servidor->setCallbacks(new CallbacksServidor());

  // 2. Crear el servei propi
  BLEService* servei = servidor->createService(SERVICE_UUID);

  // 3. Caracteristica 'credencials': nomes escriptura
  charCred = servei->createCharacteristic(CRED_UUID, BLECharacteristic::PROPERTY_WRITE);
  charCred->setCallbacks(new CallbacksCredencials());

  // 4. Caracteristica 'estat': lectura i notificacio
  charEstat = servei->createCharacteristic(
      ESTAT_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  charEstat->addDescriptor(new BLE2902());   // descriptor que permet activar les notificacions

  servei->start();

  // 5. Anunciar-se amb el UUID del servei
  BLEAdvertising* anunci = BLEDevice::getAdvertising();
  anunci->addServiceUUID(SERVICE_UUID);
  anunci->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.printf("Anunciant-me com a %s\n", NOM_BLE);
  publicaEstat("SENSE CREDENCIALS");
}

void loop() {
  M5.update();

  if (credRebudes) {
    credRebudes = false;
    // Mai imprimim el contingut: podria portar la clau de la Wi-Fi
    Serial.printf("Rebut a credencials: %d bytes\n", credText.length());
    publicaEstat("REBUT");   // prova que les notificacions arriben al mobil
  }

  delay(10);
}
