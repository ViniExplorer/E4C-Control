#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "sensors/temp_humid.hpp"

const char* ap_ssid     = "ESP32_Telemetry_AP";
const char* ap_password = "password123";

IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress brokerIP(192, 168, 4, 2);

WiFiClient espClient;
PubSubClient mqttClient(espClient);

unsigned long last_read_time = 0;
const unsigned long READ_INTERVAL = 2000;

// MQTT Callback function to handle incoming commands
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
        Serial.print("JSON Parse failed: ");
        Serial.println(error.c_str());
        return;
    }

    if (strcmp(topic, "turbine/control/brake") == 0) {
        float target_brake = doc["target_brake"];
        Serial.printf("--> ACTION: Setting brake duty cycle to %.2f%%\n", target_brake);
        // TODO: Apply PWM to brake MOSFET
    } 
    else if (strcmp(topic, "turbine/control/emergency") == 0) {
        bool estop = doc["emergency_stop"];
        bool dump = doc["dump_divert"];
        Serial.printf("--> ACTION: E-Stop Engaged: %d | Dump Diverted: %d\n", estop, dump);
        // TODO: Trigger hardware interrupts/relays
    } 
    else if (strcmp(topic, "turbine/control/safeguard") == 0) {
        float trip_volts = doc["dump_trip_voltage"];
        Serial.printf("--> ACTION: Overvoltage safeguard set to %.2fV\n", trip_volts);
        // TODO: Update local overvoltage clamping threshold variable
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
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 1 second");
            delay(1000);
        }
    }
}

void setup() {
    Serial.begin(115200);

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(ap_ssid, ap_password);

    Serial.print("Access Point started: ");
    Serial.println(ap_ssid);
    Serial.print("ESP32 AP IP address: ");
    Serial.println(WiFi.softAPIP());

    mqttClient.setServer(brokerIP, 1883);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setKeepAlive(60); // 60-second broker keepalive window
    
    setup_temp_humid();
}

void loop() {
    if (!mqttClient.connected()) {
        reconnect();
    }
    
    mqttClient.loop(); 

    unsigned long current_time = millis();
    if (current_time - last_read_time >= READ_INTERVAL) {
        last_read_time = current_time;

        float temp = 0.0;
        float humid = 0.0;
        bool valid = read_temp_humid(temp, humid);

        char payload[128];
        if (valid) {
            snprintf(payload, sizeof(payload), 
                     "{\"temperature\":%.2f, \"humidity\":%.2f}", temp, humid);
        } else {
            snprintf(payload, sizeof(payload), 
                     "{\"temperature\":0.00, \"humidity\":0.00, \"sensor_error\":true}");
        }
        
        mqttClient.publish("turbine/telemetry", payload);
    }
}