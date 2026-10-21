cmake_minimum_required(VERSION 3.22)
project(WindTurbineGroundStation CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")

find_package(PkgConfig REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

pkg_check_modules(MOSQUITTO REQUIRED IMPORTED_TARGET libmosquitto)

include(FetchContent)

# 1. Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

# 2. ImPlot
FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG        v0.16
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(implot)

# 3. nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
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
    src/embedded_broker.cpp
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
    set_target_properties(ground_station PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "WindTurbineGroundStation"
    )
endif()

install(TARGETS ground_station
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

set(CPACK_PACKAGE_NAME "WindTurbineGroundStation")
set(CPACK_PACKAGE_VENDOR "Winds of Change")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_GENERATOR "DragNDrop")

include(CPack)
