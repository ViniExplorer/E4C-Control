#pragma once

#include <vector>
#include <mutex>
#include <chrono>
#include <cmath>
#include <algorithm>

template <typename T, size_t Capacity>
class RingBuffer {
public:
    RingBuffer() : head_(0), count_(0) {
        data_.resize(Capacity, T());
    }

    void push(const T& item) {
        data_[head_] = item;
        head_ = (head_ + 1) % Capacity;
        if (count_ < Capacity) count_++;
    }

    size_t size() const { return count_; }
    size_t capacity() const { return Capacity; }

    void get_ordered_data(std::vector<T>& out) const {
        out.resize(count_);
        if (count_ == 0) return;
        
        size_t start = (head_ + Capacity - count_) % Capacity;
        for (size_t i = 0; i < count_; ++i) {
            out[i] = data_[(start + i) % Capacity];
        }
    }

private:
    std::vector<T> data_;
    size_t head_;
    size_t count_;
};

struct TelemetrySnapshot {
    double timestamp_s{0.0};
    float voltage{0.0f};          // Bus Volts
    float current{0.0f};          // Shunt Amps
    float rpm{0.0f};              // Rotor RPM
    float temperature{0.0f};      // MCU / Nacelle Deg C
    
    // Power calculations
    float power_gen_total{0.0f};  // Raw total generator power (V * I)
    float power_grid{0.0f};       // Clamped clean grid output (capped during over-voltage)
    float power_dump{0.0f};       // Bleed power directed into dump load
    float power_expected{0.0f};   // Expected generator output based on RPM curve
    
    // Hardware Status
    bool hw_estop_engaged{false};
    bool sw_estop_engaged{false};
    bool dump_mosfet_active{false};
    float brake_duty_pct{0.0f};
};

class TelemetryManager {
public:
    static constexpr size_t BUFFER_SIZE = 600; // 60s @ 10 Hz
    static constexpr float RATED_GRID_MAX_WATTS = 226.0f; // Steady target power ceiling

    TelemetryManager() : start_time_(std::chrono::steady_clock::now()) {}

    void ingest(float v, float i, float rpm, float temp, 
                 bool hw_estop, bool sw_estop, bool mosfet, float brake) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::steady_clock::now();
        double elapsed_sec = std::chrono::duration<double>(now - start_time_).count();

        TelemetrySnapshot s;
        s.timestamp_s = elapsed_sec;
        s.voltage = v;
        s.current = i;
        s.rpm = rpm;
        s.temperature = temp;
        s.hw_estop_engaged = hw_estop;
        s.sw_estop_engaged = sw_estop;
        s.dump_mosfet_active = mosfet;
        s.brake_duty_pct = brake;

        // 1. Raw gross generator electrical power
        s.power_gen_total = std::max(0.0f, v * i);

        // 2. Expected power curve derived from rotor speed
        s.power_expected = 0.00067f * (rpm * rpm);

        // 3. Grid vs Dump Load Split:
        // ONLY true E-Stops completely disconnect the grid (0W).
        // Overvoltage (> 60V) clamps the grid power to rated steady capacity (226W),
        // diverting ONLY the surplus power to the dump resistor.
        if (hw_estop || sw_estop) {
            s.power_grid = 0.0f;
            s.power_dump = s.power_gen_total;
        } else if (v > 60.0f) {
            s.power_grid = std::min(s.power_gen_total, RATED_GRID_MAX_WATTS);
            s.power_dump = std::max(0.0f, s.power_gen_total - s.power_grid);
        } else {
            s.power_grid = s.power_gen_total;
            s.power_dump = 0.0f;
        }

        latest_ = s;
        timestamps_.push(s.timestamp_s);
        volts_.push(s.voltage);
        amps_.push(s.current);
        rpms_.push(s.rpm);
        temps_.push(s.temperature);
        p_grid_.push(s.power_grid);
        p_dump_.push(s.power_dump);
        p_exp_.push(s.power_expected);
        p_gen_.push(s.power_gen_total);
    }

    TelemetrySnapshot get_latest() {
        std::lock_guard<std::mutex> lock(mutex_);
        return latest_;
    }

    void export_buffers(
        std::vector<double>& t,
        std::vector<double>& v,
        std::vector<double>& i,
        std::vector<double>& grid_p,
        std::vector<double>& exp_p) 
    {
        std::lock_guard<std::mutex> lock(mutex_);
        timestamps_.get_ordered_data(t);
        volts_.get_ordered_data(v);
        amps_.get_ordered_data(i);
        p_grid_.get_ordered_data(grid_p);
        p_exp_.get_ordered_data(exp_p);
    }

private:
    std::mutex mutex_;
    std::chrono::time_point<std::chrono::steady_clock> start_time_;
    TelemetrySnapshot latest_;

    RingBuffer<double, BUFFER_SIZE> timestamps_;
    RingBuffer<double, BUFFER_SIZE> volts_;
    RingBuffer<double, BUFFER_SIZE> amps_;
    RingBuffer<double, BUFFER_SIZE> rpms_;
    RingBuffer<double, BUFFER_SIZE> temps_;
    RingBuffer<double, BUFFER_SIZE> p_grid_;
    RingBuffer<double, BUFFER_SIZE> p_dump_;
    RingBuffer<double, BUFFER_SIZE> p_exp_;
    RingBuffer<double, BUFFER_SIZE> p_gen_;
};