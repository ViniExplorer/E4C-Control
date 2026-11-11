#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "sensors/temp_humid.hpp"
#include "sensors/voltage_current.hpp"
#include "regulator/voltage_regulator.hpp"

const char* ap_ssid     = "ESP32_Telemetry_AP";
const char* ap_password = "password123";

IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress brokerIP(192, 168, 4, 2);

WiFiClient espClient;
PubSubClient mqttClient(espClient);

unsigned long last_read_time = 0;
const unsigned long READ_INTERVAL = 100; // 100ms interval matching bench test output

static float target_voltage = 58.0f; // Track local setpoint for telemetry print

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    char message[length + 1];
    for (unsigned int i = 0; i < length; i++) {
        message[i] = (char)payload[i];
    }
    message[length] = '\0';

    Serial.printf("\n[MQTT RX] Topic: %s | Payload: %s\n", topic, message);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
        Serial.printf("JSON Parse failed: %s\n", error.c_str());
        return;
    }

    if (strcmp(topic, "turbine/control/safeguard") == 0) {
        float trip_volts = doc["dump_trip_voltage"];
        target_voltage = trip_volts;
        set_overvoltage_threshold(trip_volts);
        Serial.printf("--> ACTION: Updated Overvoltage Threshold to %.2f V\n", trip_volts);
    } 
    else if (strcmp(topic, "turbine/control/emergency") == 0) {
        bool estop = doc["emergency_stop"];
        if (estop) {
            ledcWrite(18, 255); // Clamp dump load 100% on E-stop
            Serial.println("--> ACTION: E-STOP ENGAGED (Dump load 100%)");
        }
    }
}

void reconnect() {
    while (!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection...");
        if (mqttClient.connect("ESP32_Telemetry_Node")) {
            Serial.println("connected");
            mqttClient.subscribe("turbine/control/#");
            Serial.println("Subscribed to turbine/control/#");
        } else {
            Serial.printf("failed, rc=%d try again in 1 second\n", mqttClient.state());
            delay(1000);
        }
    }
}

void setup() {
    Serial.begin(115200);

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(ap_ssid, ap_password);

    Serial.printf("AP Started: %s | IP: %s\n", ap_ssid, WiFi.softAPIP().toString().c_str());

    mqttClient.setServer(brokerIP, 1883);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setKeepAlive(60);

    setup_temp_humid();
    setup_voltage_current();

    // Link potentiometer reader to FreeRTOS PID task
    register_voltage_reader(read_potentiometer_simulated_voltage);
    setup_voltage_regulator();

    Serial.println("--- Voltage Regulator Bench & Telemetry Node Active ---");
}

void loop() {
    if (!mqttClient.connected()) {
        reconnect();
    }
    mqttClient.loop();

    unsigned long current_time = millis();
    if (current_time - last_read_time >= READ_INTERVAL) {
        last_read_time = current_time;

        float temp = 0.0f;
        float humid = 0.0f;
        read_temp_humid(temp, humid);

        float v = get_current_voltage();
        float duty = get_current_brake_duty();
        float raw_pwm = get_current_pwm_raw();
        float current = get_estimated_current(v, duty);
        float power = v * current;

        // Exact bench test serial monitor format
        Serial.printf("Bus V: %5.2f V | Target: %5.2f V | Duty: %5.1f%% (Raw PWM: %3.0f)\n",
                      v,
                      target_voltage,
                      duty,
                      raw_pwm);

        // Telemetry publish to MQTT broker
        char payload[256];
        snprintf(payload, sizeof(payload),
            "{\"voltage\":%.2f,\"current\":%.2f,\"power\":%.2f,\"temperature\":%.2f,\"humidity\":%.2f,\"brake_duty\":%.2f}",
            v, current, power, temp, humid, duty);

        mqttClient.publish("turbine/telemetry", payload);
    }
}