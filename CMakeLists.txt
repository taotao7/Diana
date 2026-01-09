cmake_minimum_required(VERSION 3.20)
project(diana VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# Options
# =============================================================================
option(DIANA_BUILD_TESTS "Build unit tests" ON)

# =============================================================================
# Platform detection
# =============================================================================
if(APPLE)
    set(DIANA_PLATFORM "macos")
elseif(UNIX)
    set(DIANA_PLATFORM "linux")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

message(STATUS "Building diana for ${DIANA_PLATFORM}")

# =============================================================================
# Dependencies via FetchContent
# =============================================================================
include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Dear ImGui (docking branch)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

# ImPlot
FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(implot)

# nlohmann/json for config parsing
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(json)

# tomlplusplus for Codex config.toml
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(tomlplusplus)

# Native File Dialog Extended for file/folder selection
FetchContent_Declare(
    nfd
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG        v1.2.1
    GIT_SHALLOW    TRUE
)
set(NFD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nfd)

# =============================================================================
# ImGui library target
# =============================================================================
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui_lib PUBLIC glfw)

find_package(OpenGL REQUIRED)

if(APPLE)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(IOKIT_LIBRARY IOKit REQUIRED)
    find_library(COREVIDEO_LIBRARY CoreVideo REQUIRED)
    target_link_libraries(imgui_lib PUBLIC
        OpenGL::GL
        ${COCOA_LIBRARY}
        ${IOKIT_LIBRARY}
        ${COREVIDEO_LIBRARY}
    )
    target_include_directories(imgui_lib PUBLIC ${OPENGL_INCLUDE_DIR})
elseif(UNIX)
    target_link_libraries(imgui_lib PUBLIC OpenGL::GL)
endif()

# =============================================================================
# ImPlot library target
# =============================================================================
add_library(implot_lib STATIC
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)

target_include_directories(implot_lib PUBLIC
    ${implot_SOURCE_DIR}
)

target_link_libraries(implot_lib PUBLIC imgui_lib)

# =============================================================================
# Main executable
# =============================================================================
add_executable(diana
    src/main.cpp
    src/app/app_shell.cpp
    src/app/dockspace.cpp
    src/terminal/ansi_parser.cpp
    src/terminal/terminal_buffer.cpp
    src/terminal/terminal_session.cpp
    src/terminal/terminal_panel.cpp
    src/process/process_runner.cpp
    src/process/session_controller.cpp
    src/adapters/claude_code_adapter.cpp
    src/adapters/codex_adapter.cpp
    src/adapters/opencode_adapter.cpp
    src/adapters/config_manager.cpp
    src/adapters/config_exporter.cpp
    src/adapters/claude_code_config.cpp
    src/adapters/claude_profile_store.cpp
    src/ui/metrics_panel.cpp
    src/ui/claude_code_panel.cpp
    src/metrics/metrics_store.cpp
    src/metrics/claude_usage_collector.cpp
)

target_include_directories(diana PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(diana PRIVATE
    imgui_lib
    implot_lib
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
    nfd
)

# =============================================================================
# Tests
# =============================================================================
if(DIANA_BUILD_TESTS)
    enable_testing()
    
    # Google Test
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
        GIT_SHALLOW    TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    
    add_executable(diana_tests
        tests/test_main.cpp
        tests/core/test_event_queue.cpp
        tests/metrics/test_metrics_store.cpp
        tests/adapters/test_config_exporter.cpp
        src/metrics/metrics_store.cpp
        src/adapters/config_exporter.cpp
    )
    
    target_include_directories(diana_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    
    target_link_libraries(diana_tests PRIVATE
        GTest::gtest
        GTest::gtest_main
        nlohmann_json::nlohmann_json
        tomlplusplus::tomlplusplus
    )
    
    include(GoogleTest)
    gtest_discover_tests(diana_tests)
endif()
