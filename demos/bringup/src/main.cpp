#include <Arduino.h>
#include <ModbusMaster.h>

#define RS485_RX_PIN 16
#define RS485_TX_PIN 17

#define PZEM_ADDRESS 0x01

ModbusMaster pzem;

void setup() {
  Serial.begin(9600);
  delay(1500);
  Serial.println("\n--- PZEM-017 Robust Reader ---");

  // PZEM requires 9600 baud, 8N2
  Serial2.begin(9600, SERIAL_8N2, RS485_RX_PIN, RS485_TX_PIN);
  pzem.begin(PZEM_ADDRESS, Serial2);
}

// Function to read PZEM with buffer clearing and retry
bool readPZEM(float &voltage, float &current, float &power) {
  for (uint8_t attempt = 0; attempt < 2; attempt++) {
    // Flush stale bytes from previous timeouts
    while (Serial2.available()) {
      Serial2.read();
    }
    delay(50); // Inter-frame quiet period for XY-017 auto-switching

    uint8_t result = pzem.readInputRegisters(0x0000, 8);

    if (result == pzem.ku8MBSuccess) {
      voltage = pzem.getResponseBuffer(0x0000) * 0.01f;
      current = pzem.getResponseBuffer(0x0001) * 0.01f;
      uint32_t powerRaw = ((uint32_t)pzem.getResponseBuffer(0x0003) << 16) | pzem.getResponseBuffer(0x0002);
      power = powerRaw * 0.1f;
      return true;
    }

    // Brief pause before retry if dropped
    delay(100);
  }
  return false;
}

void loop() {
  float voltage = 0.0f, current = 0.0f, power = 0.0f;

  if (readPZEM(voltage, current, power)) {
    Serial.printf("Voltage: %6.2f V | Current: %5.2f A | Power: %6.1f W\n", voltage, current, power);
  } else {
    Serial.println("Reading failed (retried 2x). Check power / wiring.");
  }

  delay(1500);
}