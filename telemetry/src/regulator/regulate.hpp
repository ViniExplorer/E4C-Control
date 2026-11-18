#ifndef VOLTAGE_REGULATOR_H
#define VOLTAGE_REGULATOR_H

#include <Arduino.h>

void setup_voltage_regulator();
void set_overvoltage_safeguard(float trip_volts);
float get_current_voltage();
float get_current_brake_duty();

#endif