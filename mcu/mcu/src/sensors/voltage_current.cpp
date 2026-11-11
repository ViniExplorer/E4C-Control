#include "voltage_current.hpp"

#define POT_ADC_PIN 34 // Potentiometer center leg (ADC1_CH6)

void setup_voltage_current() {
    analogReadResolution(12);
    pinMode(POT_ADC_PIN, INPUT);
    analogRead(POT_ADC_PIN); // Warm up ADC
}

// Maps 12-bit ADC (0 - 4095) to simulated 0.0V - 70.0V DC bus
float read_potentiometer_simulated_voltage() {
    uint32_t raw_adc = analogRead(POT_ADC_PIN);
    return ((float)raw_adc / 4095.0f) * 70.0f;
}

// Approximates bleed/injection current through the dummy load (assumes ~8 Ohm load)
float get_estimated_current(float voltage, float pwm_duty_pct) {
    const float LOAD_RESISTANCE = 8.0f;
    float effective_voltage = voltage * (pwm_duty_pct / 100.0f);
    return effective_voltage / LOAD_RESISTANCE;
}