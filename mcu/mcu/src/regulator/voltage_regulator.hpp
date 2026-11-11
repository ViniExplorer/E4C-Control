#pragma once
#include <Arduino.h>

typedef float (*VoltageReaderFn)();
typedef void (*PwmWriterFn)(uint8_t pin, uint32_t duty);

void setup_voltage_regulator();
void register_voltage_reader(VoltageReaderFn fn);
void register_pwm_writer(PwmWriterFn fn);
void set_overvoltage_threshold(float volts);

float get_current_voltage();
float get_current_brake_duty();
float get_current_pwm_raw();