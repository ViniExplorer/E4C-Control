#include <Arduino.h>
#include <DHT.h>
#include "temp_humid.hpp"

#define DHTPIN 15  
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

void setup_temp_humid() {
    dht.begin();
    Serial.println(F("DHT11 initialized."));
}

bool read_temp_humid(float &temp, float &humid) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
        Serial.println(F("DHT error: Failed to read from sensor!"));
        return false;
    }

    temp = t;
    humid = h;
    return true;
}