
#include <Arduino.h>
#include <ModbusMaster.h>

// ESP32-S3 UART1 pins
#define RS485_RX_PIN 18
#define RS485_TX_PIN 17

// Modbus slave addresses
#define OLD_ADDRESS 0x01
#define NEW_ADDRESS 0x02

// PZEM address configuration register
#define ADDRESS_REG 0x0002

ModbusMaster pzem;

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println("PZEM Modbus Address Configuration");

  // Initialise UART1 for RS-485 communication
  Serial1.begin(9600, SERIAL_8N1,
                RS485_RX_PIN, RS485_TX_PIN);

  // Communicate using the current slave address
  pzem.begin(OLD_ADDRESS, Serial1);

  Serial.println("Changing PZEM address from 01 to 02...");

  // Modbus function 06: Write Single Register
  uint8_t result =
      pzem.writeSingleRegister(ADDRESS_REG, NEW_ADDRESS);

  if (result == pzem.ku8MBSuccess) {
    Serial.println("Address change acknowledged.");

    // Switch to the new slave address
    pzem.begin(NEW_ADDRESS, Serial1);

    // Read the address register to verify
    uint8_t check =
        pzem.readHoldingRegisters(ADDRESS_REG, 1);

    if (check == pzem.ku8MBSuccess) {
      uint16_t address = pzem.getResponseBuffer(0);

      if (address == NEW_ADDRESS) {
        Serial.println("SUCCESS: PZEM address is now 02.");
      } else {
        Serial.println("Verification failed: Unexpected address.");
      }
    } else {
      Serial.print("Verification error: 0x");
      Serial.println(check, HEX);
    }

  } else {
    Serial.print("Address change error: 0x");
    Serial.println(result, HEX);
  }
}

void loop() {
  // No repeated writes required
}

