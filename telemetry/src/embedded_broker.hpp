#pragma once

#include <thread>
#include <atomic>
#include <string>

class EmbeddedBroker {
public:
    EmbeddedBroker() = default;
    ~EmbeddedBroker();

    bool start(const std::string& listen_addr = "mqtt-tcp://0.0.0.0:1883");
    void stop();
    bool is_running() const { return running_.load(); }

private:
    std::atomic<bool> running_{false};
    std::thread broker_thread_;
    void* nmq_ctx_{nullptr};
};