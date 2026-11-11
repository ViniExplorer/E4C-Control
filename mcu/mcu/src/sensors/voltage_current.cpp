#include "voltage_current.hpp"

#define POT_ADC_PIN 34 // Potentiometer center leg (ADC1_CH6)

void setup_voltage_current() {
    analogReadResolution(12);
    pinMode(POT_ADC_PIN, INPUT);
    analogRead(POT_ADC_PIN); // warm up ADC
}

// maps 12-bit ADC (0 - 4095) to simulated 0.0V - 70.0V dc
float read_potentiometer_simulated_voltage() {
    uint32_t raw_adc = analogRead(POT_ADC_PIN);
    return ((float)raw_adc / 4095.0f) * 70.0f;
}

// aprox bleed/injection current through the dummy load (assumes ~8 Ohm load)
// Simulates turbine generator current injected into the grid inverter (e.g., ~0 to 8A)
float get_estimated_current(float voltage, float pwm_duty_pct) {
    if (voltage < 5.0f) return 0.0f; // Cut-in threshold

    float generation_current = voltage * 0.12f;

    // Optional: subtract bleed current diverted away by the dump load
    float bleed_current = (voltage * (pwm_duty_pct / 100.0f)) / 8.0f;
    float net_grid_current = generation_current - bleed_current;

    return (net_grid_current > 0.0f) ? net_grid_current : 0.0f;
}