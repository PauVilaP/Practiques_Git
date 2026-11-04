// =====================================================================
// PESTANYA P3 - el codi de la P3 (MQTT i quadre de comandament)
// Es el p3_node.ino de la P3. Nomes canvien les linies marcades amb [P2]:
// les coses que abans la P3 feia pel seu compte i que ara ja fa la P2
// (Wi-Fi, sensor, pantalla, NVS i ordres pel port serie).
// =====================================================================
//
// P3 - Node MQTT: telemetria, periode configurable, aturar/reprendre i alertes
// Node: M5Stack AtomS3. DHT22 a G8 (com a la P1 i la P2)
//
// [P2] La Wi-Fi la porta la P2: provisioning per BLE i maquina d'estats. La P3 nomes es
//      connecta al broker quan la P2 diu que el node es OPERATIU (ja te IP).
// Ordres pel Serial Monitor (115200 baud, final de linia "New Line"):
//   broker 10.11.1.248     canvia la IP del Raspberry (es desa a la NVS)
// [P2] L'ordre "wifi NOM CLAU" ja no hi es: la Wi-Fi es configura per BLE, com a la P2,
//      i aixi la clau no passa mai pel port serie.

// [P2] Els #include son a la pestanya principal (p3_sobre_p2.ino), amb els de la P2

// ---------- Configuracio ----------
// [P2] PIN_DHT el defineix la P2
#define MQTT_USER  "node01"
#define MQTT_PASS  "posa-la-teva"
#define BROKER_PER_DEFECTE "10.11.0.246"  // IP del Raspberry (si canvia: ordre "broker")

// ---------- Temes (els del P-PRE3) ----------
#define BASE "uvic/se2627/pauvila/node01"
const char* T_TEMP     = BASE "/telemetria/temp";
const char* T_HUM      = BASE "/telemetria/hum";
const char* T_STATUS   = BASE "/status";
const char* T_TRANS    = BASE "/status/transmissio";
const char* T_CMD      = BASE "/cmd/transmissio";
const char* T_CONFIG   = BASE "/config/#";
const char* T_PERIODE  = BASE "/config/periode";
const char* T_LLINDARS = BASE "/config/llindars";
const char* T_ACK      = BASE "/ack/";      // + periode, llindars o transmissio
const char* T_ALARMA   = BASE "/alarma/";   // + temp o hum

// ---------- Objectes ----------
WiFiClient   clientWifi;
PubSubClient mqtt(clientWifi);
// [P2] El DHT22 (dht) i la NVS (nvs) son els de la P2

String brokerIp;
char   clientId[24];   // "pauvila-" + final de la MAC: unic i el mateix a cada reinici

// ---------- Estat del node ----------
int   periode = 10;          // segons entre enviaments (exercici 4)
bool  transmetent = true;    // exercici 5
float tMin = 15, tMax = 30;  // llindars (exercici 6), fins que arribin els del dashboard
float hMin = 20, hMax = 80;
bool  alarmaTemp = false, alarmaHum = false;

// [P2] temperatura i humitat son les de la P2, que llegeix el DHT22 cada 2 s
unsigned long ultimEnviament = 0;
unsigned long darrerIntent   = 0;
unsigned long espera         = 0;   // segons fins al proper intent de connexio (0 = ara)
bool          mqttAbans      = false;   // [P2] per redibuixar la linia del MQTT quan la connexio canvia


// =================================================================
// Pantalla (128x128)
// =================================================================
// [P2] La pantalla ara es de la P2 (dibuixaPantalla() a P2_configuracio.ino). La P3 hi posa
//      tres linies a sota de les de la P2 i, si hi ha alguna alarma, el fons vermell.
#define Y_MQTT    80    // connexio al broker i periode aplicat (exercicis 2 i 4), mida 1
#define Y_TRANS   92    // estat de la transmissio (exercici 5), mida 1
#define Y_ALARMA 104    // alerta (exercici 6), mida 1

// Fons de tota la pantalla: vermell si hi ha alguna alarma (abans, fillScreen(fons) de la P3)
uint16_t p3ColorFons() {
  return (alarmaTemp || alarmaHum) ? TFT_RED : TFT_BLACK;
}

// La magnitud que esta en alarma surt en groc
uint16_t p3ColorTemp() { return alarmaTemp ? TFT_YELLOW : TFT_CYAN; }
uint16_t p3ColorHum()  { return alarmaHum  ? TFT_YELLOW : TFT_CYAN; }

void dibuixaMqtt() {
  if (avisReset) return;               // [P2] durant l'avis de restabliment de la P2 no es dibuixa res
  char txt[24];
  snprintf(txt, sizeof(txt), "MQTT %s  Periode %ds", mqtt.connected() ? "ok" : "--", periode);
  pantallaLinia(Y_MQTT, txt, mqtt.connected() ? TFT_GREEN : TFT_ORANGE, 1);
}

void dibuixaTransmissio() {
  if (avisReset) return;
  pantallaLinia(Y_TRANS, transmetent ? "Transmissio: running" : "Transmissio: paused",
                transmetent ? TFT_WHITE : TFT_YELLOW, 1);
}

void dibuixaAlarma() {
  if (avisReset) return;
  const char* txt = "";
  if (alarmaTemp && alarmaHum) txt = "ALERTA: temp i hum";
  else if (alarmaTemp)         txt = "ALERTA: temperatura";
  else if (alarmaHum)          txt = "ALERTA: humitat";
  pantallaLinia(Y_ALARMA, txt, TFT_WHITE, 1);
}

// [P2] La crida dibuixaPantalla() de la P2, despres de dibuixar les seves linies
void p3DibuixaPantalla() {
  dibuixaMqtt();
  dibuixaTransmissio();
  dibuixaAlarma();
}


// =================================================================
// Publicar
// =================================================================
// Marca de temps en segons (temps Unix). Si encara no tenim l'hora per NTP, 0
unsigned long marcaTemps() {
  time_t ara = time(nullptr);
  return ara > 1700000000 ? (unsigned long)ara : 0;
}

// Telemetria: {"valor":23.4,"unitat":"C","ts":1793014800}. QoS 0 i no retinguda
void publicaMagnitud(const char* tema, float valor, const char* unitat) {
  JsonDocument doc;
  doc["valor"]  = roundf(valor * 10) / 10.0;   // un decimal
  doc["unitat"] = unitat;
  doc["ts"]     = marcaTemps();
  char msg[96];
  serializeJson(doc, msg);
  mqtt.publish(tema, msg);
  Serial.printf("[MQTT] %s %s\n", tema, msg);
}

// Acusament a .../ack/<nom>. No retingut
void enviaAck(const char* nom, JsonDocument& ack) {
  char tema[80], msg[128];
  snprintf(tema, sizeof(tema), "%s%s", T_ACK, nom);
  serializeJson(ack, msg);
  mqtt.publish(tema, msg);
  Serial.printf("[ACK] %s %s\n", tema, msg);
}

// Alarma a .../alarma/<magnitud>: {"actiu":true,"valor":31.2,"ts":...}. No retinguda
void publicaAlarma(const char* magnitud, bool actiu, float valor) {
  JsonDocument doc;
  doc["actiu"] = actiu;
  doc["valor"] = roundf(valor * 10) / 10.0;
  doc["ts"]    = marcaTemps();
  char tema[80], msg[96];
  snprintf(tema, sizeof(tema), "%s%s", T_ALARMA, magnitud);
  serializeJson(doc, msg);
  mqtt.publish(tema, msg);
  Serial.printf("[ALARMA] %s %s\n", tema, msg);
}


// =================================================================
// Rebre: configuracio i ordres
// =================================================================
// Exercici 4: {"id":"c05","parametre":"periode","valor":30}
void canviaPeriode(const char* id, JsonVariant valor) {
  JsonDocument ack;
  ack["id"] = id;
  if (!valor.is<int>()) {                                   // tipus
    ack["ok"] = false;
    ack["error"] = "ha de ser un enter";
  } else if (valor.as<int>() < 1 || valor.as<int>() > 60) { // rang
    ack["ok"] = false;
    ack["error"] = "fora de rang (1-60)";
  } else {
    periode = valor.as<int>();   // es fa servir al proper enviament: no cal reiniciar
    ack["ok"] = true;
  }
  ack["valor"] = periode;        // el periode que queda aplicat de veritat
  enviaAck("periode", ack);
}

// Exercici 6: {"id":"c06","parametre":"llindars","valor":{"tmin":15,"tmax":28,"hmin":30,"hmax":70}}
void canviaLlindars(const char* id, JsonVariant v) {
  JsonDocument ack;
  ack["id"] = id;
  bool numeros = v["tmin"].is<float>() && v["tmax"].is<float>() &&
                 v["hmin"].is<float>() && v["hmax"].is<float>();
  if (numeros && v["tmin"].as<float>() < v["tmax"].as<float>() &&
                 v["hmin"].as<float>() < v["hmax"].as<float>()) {
    tMin = v["tmin"]; tMax = v["tmax"];
    hMin = v["hmin"]; hMax = v["hmax"];
    ack["ok"] = true;
  } else {
    ack["ok"] = false;
    ack["error"] = "llindars no valids";
  }
  ack["valor"]["tmin"] = tMin;  ack["valor"]["tmax"] = tMax;
  ack["valor"]["hmin"] = hMin;  ack["valor"]["hmax"] = hMax;
  enviaAck("llindars", ack);
}

// Exercici 5: {"id":"o17","valor":"atura"} o {"id":"o18","valor":"repren"}
void canviaTransmissio(const char* id, JsonVariant valor) {
  JsonDocument ack;
  ack["id"] = id;
  String ordre = valor | "";
  if (ordre == "atura")       transmetent = false;
  else if (ordre == "repren") transmetent = true;
  else {
    ack["ok"] = false;
    ack["error"] = "ordre desconeguda";
    enviaAck("transmissio", ack);
    return;
  }
  // L'estat es publica retingut: qui obri el dashboard despres el veura igual
  mqtt.publish(T_TRANS, transmetent ? "running" : "paused", true);
  ack["ok"] = true;
  ack["valor"] = transmetent ? "running" : "paused";
  enviaAck("transmissio", ack);
}

// Es crida cada vegada que arriba un missatge a un tema on estem subscrits
void missatgeRebut(char* tema, byte* dades, unsigned int mida) {
  Serial.printf("[MQTT] Rebut %s: %.*s\n", tema, (int)mida, (char*)dades);
  JsonDocument doc;
  if (deserializeJson(doc, dades, mida)) {   // buit o JSON mal escrit
    Serial.println("  JSON no valid: no en faig cas");
    return;
  }
  const char* id = doc["id"] | "?";
  if (strcmp(tema, T_PERIODE) == 0)       canviaPeriode(id, doc["valor"]);
  else if (strcmp(tema, T_LLINDARS) == 0) canviaLlindars(id, doc["valor"]);
  else if (strcmp(tema, T_CMD) == 0)      canviaTransmissio(id, doc["valor"]);
  p3DibuixaPantalla();   // [P2] nomes les linies de la P3 (abans es redibuixava tota la pantalla)
}


// =================================================================
// Connexio al broker (exercici 2)
// =================================================================
bool connectaMqtt() {
  // Last Will: si el node desapareix, el broker publica "offline" (retingut) en nom seu
  if (!mqtt.connect(clientId, MQTT_USER, MQTT_PASS, T_STATUS, 1, true, "offline")) {
    return false;
  }
  mqtt.publish(T_STATUS, "online", true);                          // retingut
  mqtt.publish(T_TRANS, transmetent ? "running" : "paused", true); // retingut
  mqtt.subscribe(T_CMD, 1);
  mqtt.subscribe(T_CONFIG, 1);   // periode i llindars: com que son retinguts, arriben de seguida
  return true;
}

// Es crida a cada volta del loop. Mai es queda esperant
void mantenirMqtt() {
  if (mqtt.connected()) {
    mqtt.loop();                 // rep missatges i envia el keepalive
    return;
  }
  if (estat != OPERATIU) return;                           // [P2] sense IP no cal provar-ho (la Wi-Fi la porta la P2)
  if (botoPremut) return;                                  // [P2] com el POST de la P2: amb el boto premut no fem res que pugui bloquejar
  if (millis() - darrerIntent < espera * 1000UL) return;   // encara no toca
  darrerIntent = millis();

  Serial.printf("[MQTT] Connectant a %s com a %s...\n", brokerIp.c_str(), clientId);
  if (connectaMqtt()) {
    Serial.println("[MQTT] Connectat");
    espera = 0;
  } else {
    // Espera cada cop el doble: 1, 2, 4, 8... fins a 60 s. Mai un bucle sense pausa
    espera = (espera == 0) ? 1 : min(espera * 2, 60UL);
    Serial.printf("[MQTT] Error %d. Ho torno a provar d'aqui a %lu s\n", mqtt.state(), espera);
  }
  dibuixaMqtt();                 // [P2] nomes la linia del MQTT
}

// [P2] La P2 la crida quan entra a OPERATIU: ja hi ha IP, aixi que provem el broker de seguida
void p3EntraOperatiu() {
  espera = 0;
}


// =================================================================
// Sensor i alarmes (exercici 6)
// =================================================================
// [P2] El DHT22 el llegeix la P2 cada 2 s (llegeixSensor() a P2_configuracio.ino) i, despres
//      de cada lectura bona, crida p3NovaLectura()

// Compara amb els llindars. Publica quan una alarma comenca o acaba.
// Torna true si n'acaba de comencar alguna
bool comprovaAlarmes() {
  if (isnan(temperatura)) return false;
  bool comenca = false;

  bool foraT = temperatura < tMin || temperatura > tMax;
  if (foraT != alarmaTemp) {
    alarmaTemp = foraT;
    publicaAlarma("temp", alarmaTemp, temperatura);
    if (alarmaTemp) comenca = true;
  }
  bool foraH = humitat < hMin || humitat > hMax;
  if (foraH != alarmaHum) {
    alarmaHum = foraH;
    publicaAlarma("hum", alarmaHum, humitat);
    if (alarmaHum) comenca = true;
  }
  return comenca;
}

// [P2] Abans aixo era al loop() de la P3, just despres de llegir el sensor
void p3NovaLectura() {
  bool tAbans = alarmaTemp, hAbans = alarmaHum;
  unsigned long t0 = millis();
  bool novaAlerta = comprovaAlarmes();
  if (alarmaTemp != tAbans || alarmaHum != hAbans) {
    dibuixaPantalla();           // [P2] canvia el fons (vermell o negre): es redibuixa tota la pantalla
  }
  if (novaAlerta) {
    Serial.printf("[ALARMA] Alerta a pantalla %lu ms despres de la lectura\n", millis() - t0);
  }
}


// =================================================================
// Ordres pel Serial Monitor
// =================================================================
// [P2] Les ordres les llegeix la P2 (llegeixOrdre()). Si no les coneix, passa la linia a la P3.
//      Torna true si l'ordre era de la P3
bool p3Ordre(const String& linia) {
  if (!linia.startsWith("broker")) return false;
  String ip = linia.substring(6);
  ip.trim();
  IPAddress prova;                 // [P2] la IP es valida com fa l'ordre "servidor" de la P2
  if (ip.length() == 0) {
    Serial.printf("Broker actual: %s\n", brokerIp.c_str());
  } else if (!prova.fromString(ip)) {
    Serial.println("IP no valida. Exemple: broker 10.11.1.248");
  } else {
    brokerIp = ip;
    nvs.begin("mqtt", false);
    nvs.putString("ip", brokerIp);
    nvs.end();
    mqtt.disconnect();
    mqtt.setServer(brokerIp.c_str(), 1883);
    espera = 0;
    Serial.printf("Broker: %s (desat)\n", brokerIp.c_str());
  }
  return true;
}

// [P2] La part de la P3 de l'ordre "info" de la P2
void p3Info() {
  Serial.printf("MQTT: broker %s, client %s, %s (estat %d) | periode %d s | transmissio %s\n",
                brokerIp.c_str(), clientId, mqtt.connected() ? "connectat" : "desconnectat",
                mqtt.state(), periode, transmetent ? "running" : "paused");
  Serial.printf("Llindars: T %.1f-%.1f C, H %.1f-%.1f %% | alarmes: temp %s, hum %s\n",
                tMin, tMax, hMin, hMax, alarmaTemp ? "SI" : "no", alarmaHum ? "SI" : "no");
}


// =================================================================
// [P2] Abans eren el setup() i el loop() de la P3. Ara els criden el setup() i el loop()
//      de la pestanya principal, just despres dels de la P2
// =================================================================
void p3Setup() {
  // [P2] M5, Serial, DHT22 i Wi-Fi ja els ha engegat la P2
  configTime(0, 0, "pool.ntp.org");   // l'hora per a la marca de temps

  // Identificador de client: unic (MAC del xip) i estable entre reinicis
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(clientId, sizeof(clientId), "pauvila-%02X%02X%02X", mac[3], mac[4], mac[5]);

  // Broker
  nvs.begin("mqtt", true);
  brokerIp = nvs.getString("ip", BROKER_PER_DEFECTE);
  nvs.end();
  mqtt.setServer(brokerIp.c_str(), 1883);
  mqtt.setCallback(missatgeRebut);
  mqtt.setBufferSize(512);

  Serial.printf("[MQTT] Broker %s, client %s. Em connecto quan la P2 sigui OPERATIU\n",   // [P2]
                brokerIp.c_str(), clientId);
}

void p3Loop() {
  // [P2] Les ordres pel port serie i el sensor ja els fa el loop de la P2
  mantenirMqtt();

  // [P2] Si la connexio canvia (per exemple, perque la P2 ha perdut la Wi-Fi), s'actualitza la pantalla
  bool ara = mqtt.connected();
  if (ara != mqttAbans) {
    mqttAbans = ara;
    if (!ara) Serial.println("[MQTT] Connexio perduda");
    dibuixaMqtt();
  }

  // Telemetria cada "periode" segons, nomes si no esta aturada (exercicis 2, 4 i 5)
  if (transmetent && mqtt.connected() && millis() - ultimEnviament >= periode * 1000UL) {
    ultimEnviament = millis();
    if (!isnan(temperatura)) {
      publicaMagnitud(T_TEMP, temperatura, "C");
      publicaMagnitud(T_HUM, humitat, "%");
    }
  }
}
