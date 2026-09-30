// P1 - Exercici 5 - Temperatura, humitat i estat del LED a la pantalla de l'AtomS3
// LED a G7, polsador a G6 (l'altra pota a GND), DHT22 a G8

#include <M5Unified.h>
#include <DHT.h>

#define PIN_LED  7
#define PIN_BOTO 6
#define PIN_DHT  8

DHT dht(PIN_DHT, DHT22);

bool ledEnces = false;
bool rapid = false;              // false = lent, true = rapid
int botoAbans = HIGH;            // amb pull-up, el boto sense premer val HIGH
unsigned long ultimCanviLed = 0;
unsigned long ultimaLectura = 0;

void setup() {
  M5.begin();
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BOTO, INPUT_PULLUP);   // pull-up intern
  dht.begin();

  // La pantalla s'esborra NOMES un cop, aqui
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.setCursor(5, 5);
  M5.Display.print("DHT22");
  M5.Display.setCursor(5, 110);
  M5.Display.print("LENT ");
}

void loop() {
  // --- Polsador (igual que a l'exercici 3) ---
  int boto = digitalRead(PIN_BOTO);

  if (boto == LOW && botoAbans == HIGH) {
    delay(50);                          // esperem que passin els rebots
    if (digitalRead(PIN_BOTO) == LOW) { // si segueix premut, es una pulsacio de veritat
      rapid = !rapid;

      // Escrivim la cadencia a la pantalla
      M5.Display.setTextColor(WHITE, BLACK);
      M5.Display.setCursor(5, 110);
      if (rapid) {
        M5.Display.print("RAPID");
      } else {
        M5.Display.print("LENT ");      // l'espai tapa la D de "RAPID"
      }
    }
  }
  botoAbans = boto;

  // --- LED (igual que a l'exercici 3) ---
  int temps;
  if (rapid) {
    temps = 200;
  } else {
    temps = 1000;
  }

  if (millis() - ultimCanviLed >= temps) {
    ultimCanviLed = millis();
    ledEnces = !ledEnces;
    digitalWrite(PIN_LED, ledEnces);

    // Escrivim l'estat del LED a la pantalla
    M5.Display.setCursor(5, 85);
    if (ledEnces) {
      M5.Display.setTextColor(GREEN, BLACK);
      M5.Display.print("LED: ON ");     // l'espai tapa la F de "OFF"
    } else {
      M5.Display.setTextColor(RED, BLACK);
      M5.Display.print("LED: OFF");
    }
  }

  // --- Sensor: una lectura cada 2 segons ---
  if (millis() - ultimaLectura >= 2000) {
    ultimaLectura = millis();

    dht.read(true);
    float temperatura = dht.readTemperature();
    float humitat = dht.readHumidity();

    M5.Display.setTextColor(WHITE, BLACK);

    if (isnan(temperatura) || isnan(humitat)) {
      M5.Display.setCursor(5, 35);
      M5.Display.print("T: ERROR ");
      M5.Display.setCursor(5, 60);
      M5.Display.print("H: ERROR ");
    } else {
      M5.Display.setCursor(5, 35);
      M5.Display.printf("T:%5.1f C", temperatura);
      M5.Display.setCursor(5, 60);
      M5.Display.printf("H:%5.1f %%", humitat);
    }
  }
}