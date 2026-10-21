#!/usr/bin/env python3
import time
import json
import math
import random
import threading
import paho.mqtt.client as mqtt

# ----------------- Configuration -----------------
BROKER_HOST = "localhost"
BROKER_PORT = 1883
TELEMETRY_INTERVAL_S = 0.1  # 10 Hz telemetry stream

# ----------------- Turbine State -----------------
class TurbineState:
    def __init__(self):
        self.lock = threading.Lock()
        
        # Environmental wind dynamics (smooth atmospheric turbulence)
        self.wind_speed = 7.5        # Baseline steady wind (m/s)
        self.wind_drift_target = 7.5
        
        # Generator dynamics (steady nominal operating points)
        self.rpm = 570.0             # Rotor RPM (~540 - 600 RPM band)
        self.voltage = 52.2          # Bus Volts (~49 - 55V safe zone)
        self.current = 4.15          # Shunt Amps (~3.9 - 4.4A)
        self.temperature = 35.2      # MCU Nacelle temp °C
        
        # Actuator / Safety state
        self.brake_duty = 0.0        # 0 to 100%
        self.hw_estop = False        # Physical E-stop button
        self.sw_estop = False        # Remote GUI E-stop command
        self.dump_active = False     # Dump load MOSFET state

state = TurbineState()

# ----------------- MQTT Callbacks (Paho 2.x) -----------------
def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print("[EMULATOR] Connected to MQTT broker.")
        client.subscribe("turbine/control/brake")
        client.subscribe("turbine/control/emergency")
        print("[EMULATOR] Subscribed to turbine/control/# (Listening for GUI commands)")
    else:
        print(f"[EMULATOR] Connection failed with code: {reason_code}")

def on_message(client, userdata, msg):
    try:
        payload = json.loads(msg.payload.decode("utf-8"))
        topic = msg.topic
        
        with state.lock:
            if topic == "turbine/control/brake":
                state.brake_duty = float(payload.get("target_brake", state.brake_duty))
                print(f"[CMD] Brake actuator set to: {state.brake_duty:.1f}%")

            elif topic == "turbine/control/emergency":
                e_stop = bool(payload.get("emergency_stop", False))
                dump = bool(payload.get("dump_divert", False))
                state.sw_estop = e_stop
                state.dump_active = dump
                if e_stop:
                    state.brake_duty = 100.0
                    print("[ALERT] E-STOP RECEIVED: Rotor locked 100%, Dump Diverter ACTIVE.")
                else:
                    print("[CMD] Software E-Stop cleared.")

    except Exception as e:
        print(f"[ERR] Failed to process message on {msg.topic}: {e}")

# ----------------- Normal Operating Physics Loop -----------------
def physics_step(dt: float):
    with state.lock:
        # 1. Smooth, Realistic Wind Variations (Gentle low-frequency drift)
        if random.random() < 0.04:
            # Stays within gentle operational envelope: 7.1 to 7.8 m/s
            state.wind_drift_target = random.uniform(7.1, 7.8)
        
        state.wind_speed += (state.wind_drift_target - state.wind_speed) * (0.6 * dt)
        state.wind_speed += random.uniform(-0.02, 0.02)

        # 2. Rotor Dynamics under Aerodynamic Torque vs Brake
        # Nominal operating target speed: ~540 - 600 RPM
        gross_target_rpm = state.wind_speed * 76.0
        effective_target_rpm = max(0.0, gross_target_rpm * (1.0 - (state.brake_duty / 100.0)))

        # Mechanical inertia convergence
        inertia_rate = 7.0 if state.brake_duty > 50.0 else 2.5
        state.rpm += (effective_target_rpm - state.rpm) * (inertia_rate * dt)
        if state.rpm < 2.0:
            state.rpm = 0.0

        # 3. Voltage Generation (Ke = 0.0915 V/RPM)
        # Normal 570 RPM produces ~52.1V (safely below 60.0V limit)
        raw_volts = state.rpm * 0.0915
        jitter = random.uniform(-0.08, 0.08) if state.rpm > 10.0 else 0.0
        state.voltage = max(0.0, raw_volts + jitter)

        # 4. Current Drawn (Load resistance model: R_load ~ 12.6 ohms)
        if state.voltage > 1.0:
            state.current = (state.voltage / 12.6) + random.uniform(-0.02, 0.02)
        else:
            state.current = 0.0

        # 5. Over-Voltage & E-Stop Safeguard
        # Dump is only triggered if an E-stop is tripped or if voltage breaches 60V
        state.dump_active = bool(state.sw_estop or state.hw_estop or (state.voltage >= 60.0))

        # 6. Thermal Dissipation
        heat_gen = (state.current * 0.12) + (state.brake_duty * 0.05)
        dissipation = (state.temperature - 22.0) * 0.025
        state.temperature += (heat_gen - dissipation) * dt

# ----------------- Keyboard Terminal Controls -----------------
def keyboard_monitor():
    print("\n================== EMULATOR COMMANDS ==================")
    print("  'h' + Enter : Toggle turbine physical E-Stop switch")
    print("  'q' + Enter : Quit emulator")
    print("=======================================================\n")

    while True:
        try:
            cmd = input().strip().lower()
            with state.lock:
                if cmd == 'h':
                    state.hw_estop = not state.hw_estop
                    state.brake_duty = 100.0 if state.hw_estop else state.brake_duty
                    print(f"--> Physical E-Stop: {'ENGAGED (LOCKED)' if state.hw_estop else 'ARMED (NORMAL)'}")
                elif cmd == 'q':
                    return
        except (EOFError, KeyboardInterrupt):
            return

# ----------------- Main Entrypoint -----------------
def main():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="esp32_turbine_mcu_sim")
    client.on_connect = on_connect
    client.on_message = on_message

    try:
        client.connect(BROKER_HOST, BROKER_PORT, keepalive=60)
    except Exception as e:
        print(f"[FATAL] Cannot connect to broker: {e}. Make sure mosquitto is running.")
        return

    client.loop_start()

    k_thread = threading.Thread(target=keyboard_monitor, daemon=True)
    k_thread.start()

    prev_time = time.time()

    try:
        while True:
            now = time.time()
            dt = now - prev_time
            prev_time = now

            physics_step(dt)

            with state.lock:
                telemetry = {
                    "voltage": round(state.voltage, 2),
                    "current": round(state.current, 2),
                    "rpm": round(state.rpm, 1),
                    "temperature": round(state.temperature, 1),
                    "hw_estop": state.hw_estop,
                    "sw_estop": state.sw_estop,
                    "dump_active": state.dump_active,
                    "brake_duty": round(state.brake_duty, 1)
                }

            client.publish("turbine/telemetry", json.dumps(telemetry), qos=0)
            time.sleep(TELEMETRY_INTERVAL_S)

    except KeyboardInterrupt:
        print("\nStopping emulator...")
    finally:
        client.loop_stop()
        client.disconnect()

if __name__ == "__main__":
    main()