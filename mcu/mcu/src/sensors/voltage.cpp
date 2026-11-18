#include "voltage.hpp"
#include <ModbusMaster.h>

#define RS485_RX_PIN 16
#define RS485_TX_PIN 17
#define PZEM_ADDRESS 0x01

static ModbusMaster pzem;

static volatile float cached_voltage = 0.0f;
static volatile float cached_current = 0.0f;
static volatile float cached_power   = 0.0f;

static bool poll_pzem_once() {
    for (uint8_t attempt = 0; attempt < 2; attempt++) {
        while (Serial2.available()) {
            Serial2.read();
        }

        // 10ms silence period, lets the modbus know the old packet done, new one ready
        vTaskDelay(pdMS_TO_TICKS(10));

        // read only registers 0x0000..0x0003 (Voltage, Current, Power Low/High)
        uint8_t result = pzem.readInputRegisters(0x0000, 4);

        if (result == pzem.ku8MBSuccess) {
            float v = pzem.getResponseBuffer(0x0000) * 0.01f;
            float c = pzem.getResponseBuffer(0x0001) * 0.01f;
            uint32_t powerRaw = ((uint32_t)pzem.getResponseBuffer(0x0003) << 16) | pzem.getResponseBuffer(0x0002);
            float p = powerRaw * 0.1f;

            cached_voltage = v;
            cached_current = c;
            cached_power   = p;
            return true;
        }

        vTaskDelay(pdMS_TO_TICKS(25));
    }
    return false;
}

// Background task polling continuously 
static void pzem_reader_task(void* pvParameters) {
    for (;;) {
        poll_pzem_once();
        vTaskDelay(pdMS_TO_TICKS(5)); // brief pause to prevent starving lower-priority tasks
    }
}

void setup_voltage() {
    Serial2.begin(9600, SERIAL_8N2, RS485_RX_PIN, RS485_TX_PIN);
    pzem.begin(PZEM_ADDRESS, Serial2);

    // Run one immediate read before starting the background task
    poll_pzem_once();

    // Pin to Core 0 (alongside WiFi/idle) so Core 1 stays free for PID & main loop
    xTaskCreatePinnedToCore(
        pzem_reader_task,
        "PZEM_Reader",
        4096,
        NULL,
        1,
        NULL,
        0
    );
}

bool update_pzem_sensor() {
    return true;
}

float read_pzem_voltage() {
    return cached_voltage;
}

float read_pzem_current() {
    return cached_current;
}

float read_pzem_power() {
    return cached_power;
}