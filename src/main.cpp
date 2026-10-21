#include "telemetry_state.hpp"
#include "mqtt_client.hpp"
#include "gui_app.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string broker_host = "localhost";
    int broker_port = 1883;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" && i + 1 < argc) {
            broker_host = argv[++i];
        } else if (arg == "-p" && i + 1 < argc) {
            broker_port = std::stoi(argv[++i]);
        }
    }

    TelemetryManager telemetry_manager;
    MqttTelemetryClient mqtt_client(telemetry_manager);

    std::cout << "[Init] Attempting connection to MQTT broker at " 
              << broker_host << ":" << broker_port << "..." << std::endl;
    mqtt_client.connect_broker(broker_host, broker_port);

    GroundStationApp app(telemetry_manager, mqtt_client);
    if (!app.init()) {
        std::cerr << "[Fatal] Failed to initialize GLFW / ImGui context." << std::endl;
        return -1;
    }

    std::cout << "[Runtime] Ground station GUI running." << std::endl;
    app.run();

    return 0;
}