#pragma once
#include <Arduino.h>

void setup_voltage_current();
float read_potentiometer_simulated_voltage();
float get_estimated_current(float voltage, float pwm_duty_pct);