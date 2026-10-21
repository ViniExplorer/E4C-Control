#include "mqtt_client.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

MqttTelemetryClient::MqttTelemetryClient(TelemetryManager& manager) : tm_(manager) {
    mosquitto_lib_init();
    mosq_ = mosquitto_new("wind_ground_station", true, this);
    mosquitto_connect_callback_set(mosq_, on_connect);
    mosquitto_message_callback_set(mosq_, on_message);
    mosquitto_disconnect_callback_set(mosq_, on_disconnect);
}

MqttTelemetryClient::~MqttTelemetryClient() {
    disconnect_broker();
    if (mosq_) {
        mosquitto_destroy(mosq_);
    }
    mosquitto_lib_cleanup();
}

bool MqttTelemetryClient::connect_broker(const std::string& host, int port) {
    int rc = mosquitto_connect(mosq_, host.c_str(), port, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[MQTT] Failed to connect: " << mosquitto_strerror(rc) << std::endl;
        return false;
    }
    mosquitto_loop_start(mosq_);
    return true;
}

void MqttTelemetryClient::disconnect_broker() {
    if (mosq_) {
        mosquitto_loop_stop(mosq_, true);
        mosquitto_disconnect(mosq_);
        connected_.store(false);
    }
}

void MqttTelemetryClient::on_connect(struct mosquitto* mosq, void* obj, int rc) {
    auto* self = static_cast<MqttTelemetryClient*>(obj);
    if (rc == 0) {
        self->connected_.store(true);
        std::cout << "[MQTT] Connected to broker successfully. Subscribing to telemetry..." << std::endl;
        mosquitto_subscribe(mosq, nullptr, "turbine/telemetry", 0);
    } else {
        self->connected_.store(false);
        std::cerr << "[MQTT] Connection failed with code: " << rc << std::endl;
    }
}

void MqttTelemetryClient::on_disconnect(struct mosquitto*, void* obj, int) {
    auto* self = static_cast<MqttTelemetryClient*>(obj);
    self->connected_.store(false);
}

void MqttTelemetryClient::on_message(struct mosquitto*, void* obj, const struct mosquitto_message* msg) {
    if (!msg->payload) return;
    auto* self = static_cast<MqttTelemetryClient*>(obj);
    std::string payload(static_cast<const char*>(msg->payload), msg->payloadlen);
    self->handle_telemetry_payload(payload);
}

void MqttTelemetryClient::handle_telemetry_payload(const std::string& json_str) {
    try {
        auto j = json::parse(json_str);
        float v = j.value("voltage", 0.0f);
        float i = j.value("current", 0.0f);
        float rpm = j.value("rpm", 0.0f);
        float temp = j.value("temperature", 0.0f);
        bool hw_e = j.value("hw_estop", false);
        bool sw_e = j.value("sw_estop", false);
        bool mosfet = j.value("dump_active", false);
        float brake = j.value("brake_duty", 0.0f);

        tm_.ingest(v, i, rpm, temp, hw_e, sw_e, mosfet, brake);
    } catch (const std::exception& e) {
        std::cerr << "[JSON] Parsing exception: " << e.what() << std::endl;
    }
}

void MqttTelemetryClient::send_brake(float duty_pct) {
    json j;
    j["target_brake"] = duty_pct;
    std::string s = j.dump();
    mosquitto_publish(mosq_, nullptr, "turbine/control/brake", s.length(), s.c_str(), 1, false);
}

void MqttTelemetryClient::send_emergency_stop(bool active, bool dump_divert) {
    json j;
    j["emergency_stop"] = active;
    j["dump_divert"] = dump_divert;
    std::string s = j.dump();
    mosquitto_publish(mosq_, nullptr, "turbine/control/emergency", s.length(), s.c_str(), 2, false);
}

void MqttTelemetryClient::send_overvoltage_threshold(float trip_volts) {
    json j;
    j["dump_trip_voltage"] = trip_volts;
    std::string s = j.dump();
    mosquitto_publish(mosq_, nullptr, "turbine/control/safeguard", s.length(), s.c_str(), 1, false);
}