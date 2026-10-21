cmake_minimum_required(VERSION 3.22)
project(WindTurbineGroundStation CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure CMake looks in Homebrew paths for both Apple Silicon and Intel Macs
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")

find_package(PkgConfig REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

# Use PkgConfig to find libmosquitto with full imported targets
pkg_check_modules(MOSQUITTO REQUIRED IMPORTED_TARGET libmosquitto)

include(FetchContent)

# 1. Fetch Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.4
)
FetchContent_MakeAvailable(imgui)

# 2. Fetch ImPlot
FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG        v0.16
)
FetchContent_MakeAvailable(implot)

# 3. Fetch nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

set(IMGUI_SRCS
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

set(IMPLOT_SRCS
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)

add_executable(ground_station
    src/main.cpp
    src/mqtt_client.cpp
    src/gui_app.cpp
    ${IMGUI_SRCS}
    ${IMPLOT_SRCS}
)

target_include_directories(ground_station PRIVATE
    src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${implot_SOURCE_DIR}
    ${MOSQUITTO_INCLUDE_DIRS}
    /opt/homebrew/include
)

target_link_directories(ground_station PRIVATE
    ${MOSQUITTO_LIBRARY_DIRS}
    /opt/homebrew/lib
)

target_link_libraries(ground_station PRIVATE
    glfw
    OpenGL::GL
    PkgConfig::MOSQUITTO
    nlohmann_json::nlohmann_json
)

if(APPLE)
    target_link_libraries(ground_station PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreFoundation"
        "-framework CoreVideo"
    )
endif()