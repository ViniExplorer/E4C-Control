#include "regulate.hpp"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// pin and hardware constants
#define MOSFET2_PWM_PIN 18   
#define PWM_FREQ 20000          
#define PWM_RESOLUTION 8        
#define PWM_MAX_DUTY ((1 << PWM_RESOLUTION) - 1) // 255 for 8-bit

// config
static float target_voltage = 58.0f;    
static volatile float current_voltage = 0.0f;
static volatile float current_pwm_duty = 0.0f;

// filter values
static float ema_alpha = 0.15f;         
static float Kp = 5.0f;                 
static float Ki = 1.2f;                 
static float Kd = 0.5f;                 

// anti-windup limits
static const float INTEGRAL_MAX = 100.0f;
static const float INTEGRAL_MIN = -100.0f;

// concurrency control
static TaskHandle_t RegulatorTaskHandle = NULL;
static SemaphoreHandle_t configMutex = NULL;

typedef float (*VoltageReaderFn)();
typedef void (*PwmWriterFn)(uint8_t pin, uint32_t duty);

// Default placeholder reader: replace by calling register_voltage_reader(...)
static float default_read_voltage_sensor() {
    return 0.0f; 
}
static VoltageReaderFn read_voltage_sensor = default_read_voltage_sensor;

// Default low-side N-channel write driver (Active-High)
static void default_pwm_writer(uint8_t pin, uint32_t duty) {
    ledcWrite(pin, duty);
}
static PwmWriterFn write_mosfet_pwm = default_pwm_writer;


void voltage_regulator_task(void *pvParameters) {
    float integral = 0.0f;
    float prev_error = 0.0f;
    
    const TickType_t xFrequency = pdMS_TO_TICKS(10); 
    const float dt = 0.01f; 
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (true) {
        // read input voltage
        float raw_v = read_voltage_sensor();

        // 2. Exponential Moving Average (EMA) filter
        float filtered_v = (ema_alpha * raw_v) + ((1.0f - ema_alpha) * current_voltage);
        current_voltage = filtered_v;

        // Fetch target voltage safely
        float setpoint = target_voltage;
        if (configMutex != NULL && xSemaphoreTake(configMutex, 0) == pdTRUE) {
            setpoint = target_voltage;
            xSemaphoreGive(configMutex);
        }

        // 3. Error calculation (Braking/Dump load logic: trigger when V > Target)
        float error = filtered_v - setpoint;

        if (error <= 0.0f) {
            // Below threshold: shut off MOSFET completely
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

            // Clamp output to PWM resolution range
            if (output > (float)PWM_MAX_DUTY) output = (float)PWM_MAX_DUTY;
            if (output < 0.0f) output = 0.0f;

            current_pwm_duty = output;
            write_mosfet_pwm(MOSFET2_PWM_PIN, (uint32_t)output);

            prev_error = error;
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/* --- Public Configuration & Init API --- */
void setup_voltage_regulator() {
    configMutex = xSemaphoreCreateMutex();

    // Direct modern ESP32 LEDC setup
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

void register_voltage_reader(VoltageReaderFn fn) {
    if (fn != NULL) {
        read_voltage_sensor = fn;
    }
}

void register_pwm_writer(PwmWriterFn fn) {
    if (fn != NULL) {
        write_mosfet_pwm = fn;
    }
}

void set_overvoltage_safeguard(float trip_volts) {
    if (configMutex != NULL && xSemaphoreTake(configMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        target_voltage = trip_volts;
        xSemaphoreGive(configMutex);
    } else {
        target_voltage = trip_volts;
    }
}

float get_current_voltage() {
    return current_voltage;
}

float get_current_brake_duty() {
    return (current_pwm_duty / (float)PWM_MAX_DUTY) * 100.0f;
}