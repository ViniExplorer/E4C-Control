#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

/* --- Pin and Hardware Constants --- */
#define MOSFET2_PWM_PIN    18   // LED Anode (via 220 Ohm resistor)
#define POT_ADC_PIN        34   // Potentiometer Wiper (Center Leg)
#define PWM_FREQ           20000          
#define PWM_RESOLUTION     8        
#define PWM_MAX_DUTY       ((1 << PWM_RESOLUTION) - 1) // 255 for 8-bit

/* --- Configuration & State --- */
static float target_voltage = 58.0f;    
static volatile float current_voltage = 0.0f;
static volatile float current_pwm_duty = 0.0f;

/* --- Filter & Tuning Parameters --- */
static float ema_alpha = 0.15f;         
static float Kp = 5.0f;                 
static float Ki = 1.2f;                 
static float Kd = 0.5f;                 

/* --- Anti-Windup Limits --- */
static const float INTEGRAL_MAX = 100.0f;
static const float INTEGRAL_MIN = -100.0f;

/* --- Concurrency Control --- */
static TaskHandle_t RegulatorTaskHandle = NULL;
static SemaphoreHandle_t configMutex = NULL;

typedef float (*VoltageReaderFn)();
typedef void (*PwmWriterFn)(uint8_t pin, uint32_t duty);

static float default_read_voltage_sensor() {
    return 0.0f; 
}
static VoltageReaderFn read_voltage_sensor = default_read_voltage_sensor;

static void default_pwm_writer(uint8_t pin, uint32_t duty) {
    ledcWrite(pin, duty);
}
static PwmWriterFn write_mosfet_pwm = default_pwm_writer;

/* --- Core Control Loop --- */
void voltage_regulator_task(void *pvParameters) {
    float integral = 0.0f;
    float prev_error = 0.0f;
    
    const TickType_t xFrequency = pdMS_TO_TICKS(10); 
    const float dt = 0.01f; 
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (true) {
        // 1. Read input voltage via registered function
        float raw_v = read_voltage_sensor();

        // 2. Exponential Moving Average (EMA) filter
        float filtered_v = (ema_alpha * raw_v) + ((1.0f - ema_alpha) * current_voltage);
        current_voltage = filtered_v;

        // Fetch setpoint safely
        float setpoint = target_voltage;
        if (configMutex != NULL && xSemaphoreTake(configMutex, 0) == pdTRUE) {
            setpoint = target_voltage;
            xSemaphoreGive(configMutex);
        }

        // 3. Error calculation (Dump load activates when V > Target)
        float error = filtered_v - setpoint;

        if (error <= 0.0f) {
            // Below threshold: dump load OFF
            integral = 0.0f;
            prev_error = 0.0f;
            current_pwm_duty = 0.0f;
            write_mosfet_pwm(MOSFET2_PWM_PIN, 0);
        } else {
            // Accumulate integral with anti-windup clamping
            integral += (error * dt);
            if (integral > INTEGRAL_MAX) integral = INTEGRAL_MAX;
            if (integral < INTEGRAL_MIN) integral = INTEGRAL_MIN;

            // Derivative term
            float derivative = (error - prev_error) / dt;

            // PID formulation
            float output = (Kp * error) + (Ki * integral) + (Kd * derivative);

            // Clamp output to 8-bit PWM limit (0 - 255)
            if (output > (float)PWM_MAX_DUTY) output = (float)PWM_MAX_DUTY;
            if (output < 0.0f) output = 0.0f;

            current_pwm_duty = output;
            write_mosfet_pwm(MOSFET2_PWM_PIN, (uint32_t)output);

            prev_error = error;
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/* --- Public API --- */
void register_voltage_reader(VoltageReaderFn fn) {
    if (fn != NULL) read_voltage_sensor = fn;
}

void register_pwm_writer(PwmWriterFn fn) {
    if (fn != NULL) write_mosfet_pwm = fn;
}

void setup_voltage_regulator() {
    configMutex = xSemaphoreCreateMutex();

    ledcAttach(MOSFET2_PWM_PIN, PWM_FREQ, PWM_RESOLUTION);
    write_mosfet_pwm(MOSFET2_PWM_PIN, 0);

    xTaskCreatePinnedToCore(
        voltage_regulator_task,  
        "VoltagePID",            
        3072,                    
        NULL,                    
        configMAX_PRIORITIES - 1,
        &RegulatorTaskHandle,    
        1                        
    );
}

float get_current_voltage() {
    return current_voltage;
}

float get_current_brake_duty() {
    return (current_pwm_duty / (float)PWM_MAX_DUTY) * 100.0f;
}

/* --- Hardware Test Implementation --- */
// Reads GPIO 34 (0-3.3V) and normalizes it to a simulated 0.0V - 70.0V bus
float read_potentiometer_simulated_voltage() {
    uint32_t raw_adc = analogRead(POT_ADC_PIN); // 0 - 4095
    return ((float)raw_adc / 4095.0f) * 70.0f;
}

void setup() {
    Serial.begin(115200);
    delay(500);

    // Configure 12-bit ADC on GPIO 34
    analogReadResolution(12);
    pinMode(POT_ADC_PIN, INPUT);

    // Register potentiometer reader and spin up PID task
    register_voltage_reader(read_potentiometer_simulated_voltage);
    setup_voltage_regulator();

    Serial.println("--- Voltage Regulator Bench Test Started ---");
}

void loop() {
    // Print telemetry to Serial Monitor every 100ms
    Serial.printf("Bus V: %5.2f V | Target: %5.2f V | Duty: %5.1f%% (Raw PWM: %3.0f)\n",
                  get_current_voltage(),
                  target_voltage,
                  get_current_brake_duty(),
                  current_pwm_duty);
    delay(100);
}