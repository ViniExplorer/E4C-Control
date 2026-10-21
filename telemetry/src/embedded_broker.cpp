#include "embedded_broker.hpp"
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <spawn.h>

extern char **environ;

EmbeddedBroker::~EmbeddedBroker() {
    stop();
}

bool EmbeddedBroker::start(const std::string& listen_endpoint) {
    if (running_.load()) return true;

    // Check if port 1883 is already open
    FILE* check = popen("lsof -i :1883 2>/dev/null", "r");
    if (check) {
        char buf[128];
        if (fgets(buf, sizeof(buf), check) != nullptr) {
            pclose(check);
            std::cout << "[EMBEDDED BROKER] Port 1883 is already bound. Linking to active broker." << std::endl;
            running_.store(true);
            return true;
        }
        pclose(check);
    }

    std::cout << "[EMBEDDED BROKER] Spawning internal Mosquitto daemon on 0.0.0.0:1883..." << std::endl;

    pid_t pid;
    char *argv[] = {
        (char*)"mosquitto",
        (char*)"-p",
        (char*)"1883",
        nullptr
    };

    const char* path = "/opt/homebrew/sbin/mosquitto";
    if (access(path, X_OK) != 0) {
        path = "/usr/local/sbin/mosquitto";
    }

    int status = posix_spawnp(&pid, path, nullptr, nullptr, argv, environ);
    if (status != 0) {
        std::cerr << "[EMBEDDED BROKER] Failed to spawn broker process with status: " << status << std::endl;
        return false;
    }

    nmq_ctx_ = reinterpret_cast<void*>(static_cast<intptr_t>(pid));
    running_.store(true);
    return true;
}

void EmbeddedBroker::stop() {
    if (running_.load() && nmq_ctx_) {
        pid_t pid = static_cast<pid_t>(reinterpret_cast<intptr_t>(nmq_ctx_));
        std::cout << "[EMBEDDED BROKER] Stopping internal broker (PID " << pid << ")..." << std::endl;
        kill(pid, SIGTERM);
        waitpid(pid, nullptr, 0);
        nmq_ctx_ = nullptr;
        running_.store(false);
    }
}
