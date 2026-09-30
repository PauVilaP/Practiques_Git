// P1 - Exercici 3 - LED que parpelleja i polsador que canvia la velocitat
// LED a G7 i polsador a G8 (l'altra pota del polsador a GND)

#define PIN_LED  7
#define PIN_BOTO 6

bool ledEnces = false;
bool rapid = false;              // false = lent, true = rapid
int botoAbans = HIGH;            // amb pull-up, el boto sense premer val HIGH
unsigned long ultimCanviLed = 0;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BOTO, INPUT_PULLUP);   // pull-up intern
}

void loop() {
  // --- Polsador ---
  int boto = digitalRead(PIN_BOTO);

  // Detectem el moment de premer (passa de HIGH a LOW)
  if (boto == LOW && botoAbans == HIGH) {
    delay(50);                          // esperem que passin els rebots
    if (digitalRead(PIN_BOTO) == LOW) { // si segueix premut, es una pulsacio de veritat
      rapid = !rapid;

      Serial.print("Cadencia: ");
      if (rapid) {
        Serial.print("RAPIDA");
      } else {
        Serial.print("LENTA");
      }
      Serial.print("   Uptime: ");
      Serial.print(millis() / 1000);
      Serial.println(" s");
    }
  }
  botoAbans = boto;

  // --- LED ---
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
  }
}