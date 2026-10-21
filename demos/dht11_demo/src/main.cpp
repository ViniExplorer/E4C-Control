// DHT11 demo for ESP32 using DHTesp library
#include <Arduino.h>
#include "DHTesp.h"

#define DHTPIN 15  // GPIO connected to DHT11 DATA (pin labelled D15)

DHTesp dht;

void setup() {
  Serial.begin(9600);
  delay(2000);
  Serial.println(F("=== DHT11 Demo ==="));

  dht.setup(DHTPIN, DHTesp::DHT11);
  Serial.println(F("DHT initialized. Starting readings..."));
  
  Serial.print(F("Delay: "));
  Serial.print(dht.getMinimumSamplingPeriod());
  Serial.println(F("ms"));
}

void loop() {
  delay(dht.getMinimumSamplingPeriod());

  float h = dht.getHumidity();
  float t = dht.getTemperature();

  if (dht.getStatus() != DHTesp::ERROR_NONE) {
    Serial.print(F("DHT error: "));
    Serial.println(dht.getStatusString());
    return;
  }

  Serial.print(F("Humidity: "));
  Serial.print(h, 1);
  Serial.print(F("%  Temperature: "));
  Serial.print(t, 1);
  Serial.println(F(" C"));
}