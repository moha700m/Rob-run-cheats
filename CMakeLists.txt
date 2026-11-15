# CMakeLists.txt - RobRun external overlay.
#
#   cmake -B build -S . -A x64
#   cmake --build build --config Release
#   ctest --test-dir build -C Release --output-on-failure
#
# Windows: builds RobRunOverlay.exe (Dear ImGui is fetched at the pinned tag
# below) plus the test runner. Other platforms: builds only the portable core
# and its unit tests, which is what the Linux CI job uses.

cmake_minimum_required(VERSION 3.21)
project(RobRunOverlay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ROBRUN_BUILD_TESTS "Build unit tests" ON)
set(ROBRUN_IMGUI_TAG "v1.91.9" CACHE STRING "Dear ImGui git tag")
set(ROBRUN_IMGUI_DIR "" CACHE PATH "Use an existing Dear ImGui checkout instead of fetching")

# ---------------------------------------------------------------------------
# Portable core (no OS headers) - settings, logging, offsets, providers, worker
# ---------------------------------------------------------------------------
add_library(robrun_core STATIC
    src/core/Keys.cpp
    src/core/DataWorker.cpp
    src/config/Settings.cpp
    src/logging/Logger.cpp
    src/game/SdkOffsets.cpp
    src/game/DemoProvider.cpp
)
target_include_directories(robrun_core PUBLIC src)
find_package(Threads REQUIRED)
target_link_libraries(robrun_core PUBLIC Threads::Threads)

if(MSVC)
    target_compile_options(robrun_core PUBLIC /W4 /permissive- /utf-8 /MP)
else()
    target_compile_options(robrun_core PUBLIC -Wall -Wextra)
endif()

# Windows-only: live process-memory provider (ReadProcessMemory, read-only)
if(WIN32)
    add_library(robrun_memory STATIC src/game/MemoryProvider.cpp)
    target_link_libraries(robrun_memory PUBLIC robrun_core psapi)
    target_compile_definitions(robrun_memory PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(ROBRUN_BUILD_TESTS)
    enable_testing()
    set(ROBRUN_TEST_SOURCES
        tests/main.cpp
        tests/test_math.cpp
        tests/test_keys.cpp
        tests/test_settings.cpp
        tests/test_logger.cpp
        tests/test_offsets.cpp
        tests/test_demo.cpp
        tests/test_worker.cpp
    )
    if(WIN32)
        list(APPEND ROBRUN_TEST_SOURCES tests/test_process.cpp)
    endif()
    list(APPEND ROBRUN_TEST_SOURCES tests/test_repo_files.cpp)
    add_executable(RobRunTests ${ROBRUN_TEST_SOURCES})
    target_compile_definitions(RobRunTests PRIVATE ROBRUN_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_link_libraries(RobRunTests PRIVATE robrun_core)
    if(WIN32)
        target_compile_definitions(RobRunTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        target_link_libraries(RobRunTests PRIVATE robrun_memory psapi)
        if(MINGW)
            target_link_options(RobRunTests PRIVATE -static)
        endif()
    endif()

    foreach(suite IN ITEMS math validate signature keys settings logger offsets demo worker repo)
        add_test(NAME unit.${suite} COMMAND RobRunTests ${suite})
    endforeach()
    if(WIN32)
        add_test(NAME unit.process COMMAND RobRunTests process)
    endif()
endif()

# ---------------------------------------------------------------------------
# Windows application
# ---------------------------------------------------------------------------
if(WIN32)
    # Dear ImGui
    if(ROBRUN_IMGUI_DIR AND EXISTS "${ROBRUN_IMGUI_DIR}/imgui.h")
        set(IMGUI_DIR "${ROBRUN_IMGUI_DIR}")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/vendor/imgui/imgui.h")
        set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/vendor/imgui")
    else()
        include(FetchContent)
        FetchContent_Declare(imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG        ${ROBRUN_IMGUI_TAG}
            GIT_SHALLOW    TRUE)
        FetchContent_MakeAvailable(imgui)
        set(IMGUI_DIR "${imgui_SOURCE_DIR}")
    endif()
    message(STATUS "Dear ImGui: ${IMGUI_DIR}")

    add_library(imgui_lib STATIC
        "${IMGUI_DIR}/imgui.cpp"
        "${IMGUI_DIR}/imgui_draw.cpp"
        "${IMGUI_DIR}/imgui_tables.cpp"
        "${IMGUI_DIR}/imgui_widgets.cpp"
        "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
        "${IMGUI_DIR}/backends/imgui_impl_dx11.cpp"
    )
    target_include_directories(imgui_lib PUBLIC "${IMGUI_DIR}" "${IMGUI_DIR}/backends")
    target_compile_definitions(imgui_lib PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)

    add_executable(RobRunOverlay WIN32
        src/app/main.cpp
        src/app/Application.cpp
        src/platform/GfxWindow.cpp
        src/overlay/Overlay.cpp
        src/ui/ControlWindow.cpp
        src/ui/ControlPanel.cpp
        src/ui/EspRenderer.cpp
    )
    target_link_libraries(RobRunOverlay PRIVATE robrun_core robrun_memory imgui_lib
        d3d11 dxgi d3dcompiler dwmapi psapi shell32 user32 gdi32 ole32)
    target_compile_definitions(RobRunOverlay PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

    if(MINGW)
        # Only used for cross-compile sanity builds; the official build is MSVC.
        target_link_options(RobRunOverlay PRIVATE -municode -static)
    endif()

    if(MSVC)
        # Static CRT: single self-contained exe, no VC++ redistributable needed.
        set_property(TARGET RobRunOverlay RobRunTests robrun_core robrun_memory imgui_lib PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    add_test(NAME app.selftest COMMAND RobRunOverlay --selftest --report ${CMAKE_BINARY_DIR}/selftest_report.txt)
    set_tests_properties(app.selftest PROPERTIES TIMEOUT 120)
endif()
