#pragma once

#include <GLFW/glfw3.h>
#include "telemetry_state.hpp"
#include "mqtt_client.hpp"

class GroundStationApp {
public:
    GroundStationApp(TelemetryManager& tm, MqttTelemetryClient& mqtt);
    ~GroundStationApp();

    bool init();
    void run();

private:
    void render_ui();
    void render_status_bar();
    void render_telemetry_cards(const TelemetrySnapshot& s);
    void render_plots();
    void render_control_panel(const TelemetrySnapshot& s);

    GLFWwindow* window_{nullptr};
    TelemetryManager& tm_;
    MqttTelemetryClient& mqtt_;

    // UI state
    float target_brake_slider_{0.0f};
    float overvoltage_limit_{58.5f};
    bool sw_estop_latched_{false};
    char broker_addr_[64]{"localhost"};
    int broker_port_{1883};
};