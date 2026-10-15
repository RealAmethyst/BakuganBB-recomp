cmake_minimum_required(VERSION 3.20)
project(BattleBrawlersSource LANGUAGES CXX)

# This target contains only independently authored, SDK-free support code.
# Native integration is opt-in and requires external private inputs.
find_package(Threads REQUIRED)
add_library(brawlers_core STATIC src/core/game_speech_queue.cpp src/core/spatial_audio.cpp
    src/core/settings_menu.cpp src/core/settings_events.cpp src/core/settings_catalog.cpp)
target_sources(brawlers_core PRIVATE src/game/native_settings_save.cpp)
target_compile_features(brawlers_core PUBLIC cxx_std_17)
target_include_directories(brawlers_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game")
target_link_libraries(brawlers_core PUBLIC Threads::Threads)
set_target_properties(brawlers_core PROPERTIES CXX_EXTENSIONS OFF)

include(CTest)
if(BUILD_TESTING)
    foreach(fixture IN ITEMS guest_ui_memory game_speech_queue native_localization spatial_audio settings_menu settings_events settings_save settings_catalog)
        add_executable(test_${fixture} tests/native/test_${fixture}.cpp)
        target_link_libraries(test_${fixture} PRIVATE brawlers_core)
        # These fixtures use assert for verification, including in Release builds.
        if(MSVC)
            target_compile_options(test_${fixture} PRIVATE /UNDEBUG)
        else()
            target_compile_options(test_${fixture} PRIVATE -UNDEBUG)
        endif()
        add_test(NAME ${fixture} COMMAND test_${fixture})
    endforeach()

    # Launcher fixtures require Linux locking and the isolated Linux runner.
    # Other platforms can build the core, but have not been validated here.
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
        add_test(NAME python_contracts
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_tests.py" --python-only)
    endif()
endif()

option(BRAWLERS_BUILD_NATIVE "Build the native host using external SDK and generated inputs" OFF)
if(BRAWLERS_BUILD_NATIVE)
    include(cmake/NativeGame.cmake)
endif()
