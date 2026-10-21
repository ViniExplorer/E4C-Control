#define GL_SILENCE_DEPRECATION

#include "gui_app.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"

#include <iostream>
#include <algorithm>

GroundStationApp::GroundStationApp(TelemetryManager& tm, MqttTelemetryClient& mqtt) 
    : tm_(tm), mqtt_(mqtt) {}

GroundStationApp::~GroundStationApp() {
    ImPlot::DestroyContext();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (window_) glfwDestroyWindow(window_);
    glfwTerminate();
}

bool GroundStationApp::init() {
    if (!glfwInit()) return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    window_ = glfwCreateWindow(1440, 920, "Wind Turbine Telemetry & Control Station [ESP32-S3]", nullptr, nullptr);
    if (!window_) return false;

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 2.0f;
    style.FrameRounding = 2.0f;
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.09f, 0.10f, 1.0f);

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    return true;
}

void GroundStationApp::run() {
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        render_ui();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.08f, 0.09f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window_);
    }
}

void GroundStationApp::render_ui() {
    TelemetrySnapshot s = tm_.get_latest();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("MasterGroundStation", nullptr, 
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_MenuBar);

    render_status_bar();
    render_telemetry_cards(s);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Columns(2, "MainLayout", false);
    ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.72f);

    render_plots();

    ImGui::NextColumn();

    render_control_panel(s);

    ImGui::Columns(1);
    ImGui::End();
}

void GroundStationApp::render_status_bar() {
    if (ImGui::BeginMenuBar()) {
        bool connected = mqtt_.is_connected();
        if (connected) {
            ImGui::TextColored(ImVec4(0.2f, 0.95f, 0.3f, 1.0f), "[ONLINE] MQTT LINK ACTIVE");
        } else {
            ImGui::TextColored(ImVec4(0.95f, 0.25f, 0.25f, 1.0f), "[OFFLINE] MQTT DISCONNECTED");
        }

        ImGui::SameLine(220);
        ImGui::SetNextItemWidth(120);
        ImGui::InputText("Broker", broker_addr_, sizeof(broker_addr_));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        ImGui::InputInt("Port", &broker_port_, 0);
        ImGui::SameLine();
        if (!connected) {
            if (ImGui::Button("Connect")) mqtt_.connect_broker(broker_addr_, broker_port_);
        } else {
            if (ImGui::Button("Disconnect")) mqtt_.disconnect_broker();
        }

        ImGui::EndMenuBar();
    }
}

void GroundStationApp::render_telemetry_cards(const TelemetrySnapshot& s) {
    ImGui::BeginTable("Cards", 5, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchProp);
    
    auto draw_metric_card = [](const char* label, float value, const char* unit, const ImVec4& val_col) {
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", label);
        ImGui::SetWindowFontScale(1.4f);
        ImGui::TextColored(val_col, "%.2f", value);
        ImGui::SameLine();
        ImGui::SetWindowFontScale(0.95f);
        ImGui::TextDisabled("%s", unit);
        ImGui::SetWindowFontScale(1.0f);
    };

    draw_metric_card("VOLTAGE", s.voltage, "V", (s.voltage >= 60.0f) ? ImVec4(1.0f, 0.25f, 0.25f, 1.0f) : ImVec4(0.3f, 0.85f, 1.0f, 1.0f));
    draw_metric_card("CURRENT", s.current, "A", ImVec4(0.4f, 0.95f, 0.4f, 1.0f));
    draw_metric_card("GRID POWER", s.power_grid, "W", (s.power_grid > 0.0f) ? ImVec4(1.0f, 0.85f, 0.2f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    draw_metric_card("EXPECTED POWER", s.power_expected, "W", ImVec4(0.55f, 0.75f, 1.0f, 1.0f));
    draw_metric_card("ROTOR SPEED", s.rpm, "RPM", ImVec4(0.9f, 0.45f, 0.95f, 1.0f));

    ImGui::EndTable();
}

void GroundStationApp::render_plots() {
    std::vector<double> t;
    std::vector<double> v, i, grid_p, exp_p;
    tm_.export_buffers(t, v, i, grid_p, exp_p);

    if (t.empty()) {
        ImGui::Text("Awaiting telemetry stream frames...");
        return;
    }

    int count = static_cast<int>(t.size());
    double current_time = t.back();
    double window_span = 30.0;
    double t_min = std::max(0.0, current_time - window_span);
    double t_max = std::max(window_span, current_time);

    float avail_h = ImGui::GetContentRegionAvail().y - 10.0f;
    float plot_h = (avail_h - 10.0f) * 0.5f;

    ImPlotFlags plot_flags = ImPlotFlags_NoMouseText | ImPlotFlags_NoInputs;
    ImPlotAxisFlags x_axis_flags = ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoInitialFit;
    ImPlotAxisFlags y_axis_flags = ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoInitialFit;

    // --- GRAPH 1: Grid Injected Power vs Expected Power ---
    if (ImPlot::BeginPlot("Grid Injected Power vs Expected Power", ImVec2(-1, plot_h), plot_flags)) {
        ImPlot::SetupAxes("Elapsed Time (s)", "Power (Watts)", x_axis_flags, y_axis_flags);
        ImPlot::SetupAxisLimits(ImAxis_X1, t_min, t_max, ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 420.0, ImPlotCond_Always);

        ImPlot::PushStyleColor(ImPlotCol_Fill, ImVec4(0.15f, 0.75f, 0.25f, 0.35f));
        ImPlot::PlotShaded("Actual Grid Power (W)", t.data(), grid_p.data(), count, 0.0);
        ImPlot::PlotLine("Actual Grid Power (W)", t.data(), grid_p.data(), count);
        ImPlot::PopStyleColor();

        ImPlot::PlotLine("Expected Power (W)", t.data(), exp_p.data(), count);

        ImPlot::EndPlot();
    }

    ImGui::Spacing();

    // --- GRAPH 2: Bus Voltage & Shunt Current ---
    if (ImPlot::BeginPlot("Voltage & Current", ImVec2(-1, plot_h), plot_flags)) {
        ImPlot::SetupAxes("Elapsed Time (s)", "Bus Voltage (V)", x_axis_flags, y_axis_flags);
        ImPlot::SetupAxis(ImAxis_Y2, "Current (A)", ImPlotAxisFlags_AuxDefault | y_axis_flags);
        ImPlot::SetupAxisLimits(ImAxis_X1, t_min, t_max, ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 78.0, ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y2, 0.0, 8.5, ImPlotCond_Always);

        ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);
        ImPlot::PlotLine("Voltage", t.data(), v.data(), count);

        static double hard_limit = 60.0;
        ImPlot::DragLineY(1001, &hard_limit, ImVec4(1.0f, 0.15f, 0.15f, 0.95f), 1.5f, ImPlotDragToolFlags_NoInputs);
        ImPlot::Annotation(current_time, 60.0, ImVec4(1.0f, 0.15f, 0.15f, 1.0f), ImVec2(5, -6), true, "60V MAX GRID LIMIT");

        ImPlot::SetAxes(ImAxis_X1, ImAxis_Y2);
        ImPlot::PlotLine("Current", t.data(), i.data(), count);

        ImPlot::EndPlot();
    }
}

void GroundStationApp::render_control_panel(const TelemetrySnapshot& s) {
    ImGui::TextDisabled("HARDWARE & ROUTING STATUS");
    ImGui::Separator();

    // 1. SAFETY LOOP BADGES
    if (s.hw_estop_engaged) {
        ImGui::TextColored(ImVec4(1.0f, 0.15f, 0.15f, 1.0f), "[!] PHYSICAL E-STOP ACTIVE ON TURBINE");
    } else {
        ImGui::TextColored(ImVec4(0.2f, 0.95f, 0.3f, 1.0f), "[OK] Physical Switch Armed");
    }

    ImGui::Spacing();

    // 2. DUMP LOAD STATUS BANNER
    ImGui::TextDisabled("DUMP LOAD STATUS");
    if (s.sw_estop_engaged || s.hw_estop_engaged) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.45f, 0.10f, 0.10f, 0.95f));
        ImGui::BeginChild("DumpAlertBox", ImVec2(-1, 60), true);
        ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "[!] E-STOP TRIGGERED: 100%% DIVERT");
        ImGui::Text("Grid Disconnected. Dissipating: %.1f W", s.power_dump);
        ImGui::EndChild();
        ImGui::PopStyleColor();
    } else if (s.voltage >= 60.0f) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.35f, 0.22f, 0.08f, 0.95f));
        ImGui::BeginChild("DumpAlertBox", ImVec2(-1, 60), true);
        ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.15f, 1.0f), "[*] SHUNT ACTIVE: VOLTAGE CLAMPED");
        ImGui::Text("Grid Steady: %.1f W | Bleeding: %.1f W", s.power_grid, s.power_dump);
        ImGui::EndChild();
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.20f, 0.12f, 0.75f));
        ImGui::BeginChild("DumpAlertBox", ImVec2(-1, 50), true);
        ImGui::TextColored(ImVec4(0.2f, 0.95f, 0.4f, 1.0f), "[OK] NORMAL GRID INJECTION");
        ImGui::Text("Bus Normal (< 60V). Bleed: 0.0 W");
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Separator();

    // 3. EMERGENCY SHUTDOWN
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.05f, 0.05f, 1.0f));

    if (ImGui::Button("EMERGENCY SHUTDOWN\n(100% BRAKE + DUMP MOSFET)", ImVec2(-1, 50))) {
        sw_estop_latched_ = true;
        target_brake_slider_ = 100.0f;
        mqtt_.send_emergency_stop(true, true);
    }
    ImGui::PopStyleColor(3);

    if (sw_estop_latched_) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "LATCH ENGAGED: Commands Locked.");
        if (s.hw_estop_engaged) {
            ImGui::TextDisabled("Clear physical button before resetting software latch.");
        } else if (ImGui::Button("Reset Software Latch", ImVec2(-1, 26))) {
            sw_estop_latched_ = false;
            mqtt_.send_emergency_stop(false, false);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // 4. BRAKING CONTROL
    ImGui::BeginDisabled(sw_estop_latched_ || s.hw_estop_engaged);
    ImGui::Text("Brake Actuator (Duty Cycle):");
    
    if (ImGui::SliderFloat("##BrakeSlider", &target_brake_slider_, 0.0f, 100.0f, "%.0f%%")) {
        mqtt_.send_brake(target_brake_slider_);
    }

    if (ImGui::Button("0% Free"))   { target_brake_slider_ = 0.0f;   mqtt_.send_brake(0.0f); }
    ImGui::SameLine();
    if (ImGui::Button("25%"))       { target_brake_slider_ = 25.0f;  mqtt_.send_brake(25.0f); }
    ImGui::SameLine();
    if (ImGui::Button("50%"))       { target_brake_slider_ = 50.0f;  mqtt_.send_brake(50.0f); }
    ImGui::SameLine();
    if (ImGui::Button("75%"))       { target_brake_slider_ = 75.0f;  mqtt_.send_brake(75.0f); }
    ImGui::SameLine();
    if (ImGui::Button("100% Lock")) { target_brake_slider_ = 100.0f; mqtt_.send_brake(100.0f); }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::Separator();

    // 5. SYSTEM FEEDBACK
    ImGui::Text("Brake Duty Feedback: %.1f%%", s.brake_duty_pct);
    ImGui::Text("Nacelle Temp:        %.1f °C", s.temperature);
}