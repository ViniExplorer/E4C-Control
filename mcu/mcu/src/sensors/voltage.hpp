#ifndef VOLTAGE_HPP
#define VOLTAGE_HPP

#include <Arduino.h>

// init Serial2 (RS485) and the PZEM-017 Modbus node
void setup_voltage();

// polls the PZEM-017 over Modbus
bool update_pzem_sensor();

// Non-blocking getters returning the latest cached PZEM values
float read_pzem_voltage();
float read_pzem_current();
float read_pzem_power();

#endif 