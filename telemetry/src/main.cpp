#include "telemetry_state.hpp"
#include "mqtt_client.hpp"
#include "gui_app.hpp"
#include "embedded_broker.hpp"

#include <iostream>
#include <string>
#include <chrono>
#include <thread>

int main(int argc, char* argv[]) {
    std::string broker_host = "127.0.0.1";
    int broker_port = 1883;
    bool run_embedded_broker = true;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" && i + 1 < argc) {
            broker_host = argv[++i];
            // If user explicitly specifies a remote host, don't spin up local embedded broker
            if (broker_host != "localhost" && broker_host != "127.0.0.1") {
                run_embedded_broker = false;
            }
        } else if (arg == "-p" && i + 1 < argc) {
            broker_port = std::stoi(argv[++i]);
        } else if (arg == "--no-embedded-broker") {
            run_embedded_broker = false;
        }
    }

    // 1. Initialize & Start Embedded Broker
    EmbeddedBroker broker;
    if (run_embedded_broker) {
        std::string listen_endpoint = "mqtt-tcp://0.0.0.0:" + std::to_string(broker_port);
        std::cout << "[Init] Starting embedded MQTT broker on " << listen_endpoint << "..." << std::endl;
        
        if (!broker.start(listen_endpoint)) {
            std::cerr << "[Warn] Could not start embedded broker. Port may already be in use." << std::endl;
        } else {
            // Give socket thread 100ms to bind interface
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    // 2. Initialize Telemetry State and Client
    TelemetryManager telemetry_manager;
    MqttTelemetryClient mqtt_client(telemetry_manager);

    std::cout << "[Init] Attempting connection to MQTT broker at " 
              << broker_host << ":" << broker_port << "..." << std::endl;
    mqtt_client.connect_broker(broker_host, broker_port);

    // 3. Launch GUI
    GroundStationApp app(telemetry_manager, mqtt_client);
    if (!app.init()) {
        std::cerr << "[Fatal] Failed to initialize GLFW / ImGui context." << std::endl;
        if (run_embedded_broker) broker.stop();
        return -1;
    }

    std::cout << "[Runtime] Ground station GUI running." << std::endl;
    app.run();

    // 4. Clean Shutdown
    std::cout << "[Teardown] Stopping MQTT client and embedded broker..." << std::endl;
    mqtt_client.disconnect_broker();
    if (run_embedded_broker) {
        broker.stop();
    }

    return 0;
}