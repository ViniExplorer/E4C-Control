#pragma once

#include <mosquitto.h>
#include <string>
#include <functional>
#include <atomic>
#include "telemetry_state.hpp"

class MqttTelemetryClient {
public:
    MqttTelemetryClient(TelemetryManager& manager);
    ~MqttTelemetryClient();

    bool connect_broker(const std::string& host, int port = 1883);
    void disconnect_broker();

    // Command API
    void send_brake(float duty_pct);
    void send_emergency_stop(bool active, bool dump_divert);
    void send_overvoltage_threshold(float trip_volts);

    bool is_connected() const { return connected_.load(); }

private:
    static void on_connect(struct mosquitto* mosq, void* obj, int rc);
    static void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg);
    static void on_disconnect(struct mosquitto* mosq, void* obj, int rc);

    void handle_telemetry_payload(const std::string& json_str);

    struct mosquitto* mosq_{nullptr};
    TelemetryManager& tm_;
    std::atomic<bool> connected_{false};
};