/*
#include <Arduino.h>
#include <ModbusMaster.h>

// Hardware UART2 on ESP-32S
#define RS485_RX_PIN 16  // P16 on your board -> TXD on XY-017
#define RS485_TX_PIN 17  // P17 on your board -> RXD on XY-017

// PZEM default address is 0x01 (or 0xF8 for general address)
#define PZEM_ADDRESS 0x01

ModbusMaster pzem;

void setup() {
  Serial.begin(9600);
  delay(1500);

  Serial.println("\n--- PZEM-017 Communication Test ---");

  // Initialise Hardware Serial2 at 9600 baud, 8 data bits, 2 stop bits (per datasheet)
  Serial2.begin(9600, SERIAL_8N2, RS485_RX_PIN, RS485_TX_PIN);

  pzem.begin(PZEM_ADDRESS, Serial2);
}

void loop() {
  Serial.print("Querying PZEM-017... ");

  // Read 8 input registers starting from 0x0000 (Function 0x04)
  uint8_t result = pzem.readInputRegisters(0x0000, 8);

  if (result == pzem.ku8MBSuccess) {
    float voltage = pzem.getResponseBuffer(0x0000) * 0.01f;
    float current = pzem.getResponseBuffer(0x0001) * 0.01f;
    uint32_t powerRaw = ((uint32_t)pzem.getResponseBuffer(0x0003) << 16) | pzem.getResponseBuffer(0x0002);
    float power = powerRaw * 0.1f;

    Serial.println("SUCCESS!");
    Serial.printf("Voltage: %.2f V | Current: %.2f A | Power: %.1f W\n", voltage, current, power);
  } else {
    Serial.printf("Error: 0x%02X ", result);
    if (result == pzem.ku8MBResponseTimedOut) {
      Serial.println("(Timeout - Check 5V power to PZEM and A/B polarity)");
    } else {
      Serial.println();
    }
  }

  delay(2000);
}
*/