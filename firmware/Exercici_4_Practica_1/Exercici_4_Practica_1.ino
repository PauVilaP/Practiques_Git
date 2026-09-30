// P1 - Exercici 4 - Sensor DHT22 amb M5 AtomS3
// Cable del mig (dades) connectat a G8

#include <DHT.h>

#define PIN_DHT 8
#define TEMPS_ENTRE_LECTURES 2000   // en ms. Per a la prova rapida, posa 1000 o 500

DHT dht(PIN_DHT, DHT22);

int lectures = 0;
int fallides = 0;

void setup() {
  Serial.begin(115200);
  dht.begin();
  delay(2000);   // el sensor necessita un temps per arrencar
  Serial.println("Comencem a llegir el DHT22");
}

void loop() {
  dht.read(true);   // true = obliga a llegir el sensor de veritat

  float temperatura = dht.readTemperature();
  float humitat = dht.readHumidity();

  lectures++;

  // Si la lectura falla, el sensor retorna NaN
  if (isnan(temperatura) || isnan(humitat)) {
    fallides++;
    Serial.print("Lectura ");
    Serial.print(lectures);
    Serial.println(": ERROR");
  } else {
    Serial.print("Lectura ");
    Serial.print(lectures);
    Serial.print(": T = ");
    Serial.print(temperatura);
    Serial.print(" C   H = ");
    Serial.print(humitat);
    Serial.println(" %");
  }

  // Cada 100 lectures, mostrem quantes han fallat i tornem a comptar
  if (lectures == 100) {
    Serial.print("Han fallat ");
    Serial.print(fallides);
    Serial.println(" de 100 lectures");
    lectures = 0;
    fallides = 0;
  }

  delay(TEMPS_ENTRE_LECTURES);
}