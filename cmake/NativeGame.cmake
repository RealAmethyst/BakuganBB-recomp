# Pinned native integration contract. Generated game code stays outside this tree.
if(CMAKE_VERSION VERSION_LESS 3.25)
    message(FATAL_ERROR "Native integration requires CMake 3.25 or newer")
endif()
if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Windows)$" OR
   NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$" OR
   NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Native integration supports Linux and Windows x86-64 builds")
endif()
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
   CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    message(FATAL_ERROR "Native integration requires Clang; configure with -DCMAKE_CXX_COMPILER=clang++")
endif()

file(REAL_PATH "${PROJECT_SOURCE_DIR}" _brawlers_source_root)
file(REAL_PATH "${CMAKE_BINARY_DIR}" _brawlers_binary_root)
cmake_path(IS_PREFIX _brawlers_source_root "${_brawlers_binary_root}" NORMALIZE _brawlers_in_tree)
if(_brawlers_in_tree)
    message(FATAL_ERROR "The native build directory must be outside the repository")
endif()

set(BRAWLERS_REXGLUE_SOURCE_DIR "" CACHE PATH "External ReXGlue source with all ordered project patches applied")
set(BRAWLERS_REXGLUE_BUILD_DIR "" CACHE PATH "Matching external ReXGlue build directory with generated headers")
set(BRAWLERS_REXGLUE_OUTPUT_DIR "" CACHE PATH "Matching external Release SDK library directory")
set(BRAWLERS_REXGLUE_INSTALL_DIR "" CACHE PATH "External installed Release ReXGlue SDK (Windows)")
set(BRAWLERS_GENERATED_DIR "" CACHE PATH "External generated translation directory containing sources.cmake")
set(BRAWLERS_STEAM_AUDIO_SDK_DIR "" CACHE PATH "Optional external Steam Audio 4.8.1 SDK for spatial accessibility cues")

set(_external_directories BRAWLERS_REXGLUE_SOURCE_DIR BRAWLERS_GENERATED_DIR)
if(WIN32)
    list(APPEND _external_directories BRAWLERS_REXGLUE_INSTALL_DIR)
else()
    list(APPEND _external_directories BRAWLERS_REXGLUE_BUILD_DIR BRAWLERS_REXGLUE_OUTPUT_DIR)
endif()
foreach(_name IN LISTS _external_directories)
    if(NOT IS_DIRECTORY "${${_name}}")
        message(FATAL_ERROR "${_name} must name an existing external directory")
    endif()
    file(REAL_PATH "${${_name}}" ${_name})
    cmake_path(IS_PREFIX _brawlers_source_root "${${_name}}" NORMALIZE _brawlers_in_tree)
    if(_brawlers_in_tree)
        message(FATAL_ERROR "${_name} must be outside the repository")
    endif()
endforeach()

set(_sdk "${BRAWLERS_REXGLUE_SOURCE_DIR}")
set(_sdk_build "${BRAWLERS_REXGLUE_BUILD_DIR}")
set(_sdk_output "${BRAWLERS_REXGLUE_OUTPUT_DIR}")
set(_generated "${BRAWLERS_GENERATED_DIR}")
set(_required_files
    "${_generated}/sources.cmake"
    "${_generated}/battle_brawlers_init.h"
    "${_generated}/battle_brawlers_pch.h"
    "${_sdk}/include/rex/ppc.h"
    "${_sdk}/include/rex/system/xmemory.h"
    "${_sdk}/include/rex/ui/keyboard_dialog.h"
    "${_sdk}/include/rex/ui/message_box_dialog.h"
    "${_sdk}/include/rex/ui/modal_gamepad_input.h"
    "${_sdk}/thirdparty/inja/third_party/include/nlohmann/json.hpp")
if(WIN32)
    # Use the pinned SDK's exported dependencies, entrypoint and DLL staging.
    set(rexglue_DIR "${BRAWLERS_REXGLUE_INSTALL_DIR}/lib/cmake/rexglue")
    find_package(rexglue CONFIG REQUIRED PATHS "${rexglue_DIR}" NO_DEFAULT_PATH)
    if(NOT REXGLUE_USE_VULKAN)
        message(FATAL_ERROR "The installed ReXGlue SDK must enable Vulkan")
    endif()
else()
    list(APPEND _required_files
    "${_sdk}/src/ui/rex_app.cpp"
    "${_sdk}/src/ui/windowed_app_main_sdl.cpp"
    "${_sdk_build}/include/rex/version.h"
    "${_sdk_output}/librexruntime.so"
    "${_sdk_output}/librexgpu-xenos.so"
    "${_sdk_output}/libfmt.a"
    "${_sdk_output}/libspdlog.a"
    "${_sdk_output}/libSDL3.a")
endif()
foreach(_file IN LISTS _required_files)
    if(NOT EXISTS "${_file}" OR IS_DIRECTORY "${_file}")
        message(FATAL_ERROR "Native integration input is missing: ${_file}")
    endif()
endforeach()

# The generator's broader rexglue.cmake assumes in-tree code generation and
# metadata staging. Only its explicit translation source list is consumed here.
include("${_generated}/sources.cmake")
if(NOT GENERATED_SOURCES)
    message(FATAL_ERROR "The external sources.cmake did not provide GENERATED_SOURCES")
endif()
foreach(_file IN LISTS GENERATED_SOURCES)
    if(NOT EXISTS "${_file}" OR IS_DIRECTORY "${_file}")
        message(FATAL_ERROR "Generated translation source is missing: ${_file}")
    endif()
    file(REAL_PATH "${_file}" _actual_source)
    cmake_path(IS_PREFIX _generated "${_actual_source}" NORMALIZE _inside_generated)
    if(NOT _inside_generated)
        message(FATAL_ERROR "Generated translation sources must remain inside BRAWLERS_GENERATED_DIR: ${_file}")
    endif()
endforeach()

if(NOT WIN32)
    set(_sdk_includes
        "${_sdk}/include"
        "${_sdk_build}/include"
        "${_sdk}/thirdparty"
        "${_sdk}/thirdparty/simde"
        "${_sdk}/thirdparty/simde/simde"
        "${_sdk}/thirdparty/fmt/include"
        "${_sdk}/thirdparty/spdlog/include"
        "${_sdk_build}/thirdparty/sdl3/include-revision"
        "${_sdk}/thirdparty/sdl3/include"
        "${_sdk}/thirdparty/libmspack/cabextract/mspack"
        "${_sdk}/thirdparty/xxHash"
        "${_sdk}/thirdparty/vulkan-headers/include"
        "${_sdk}/thirdparty/vulkan-memory-allocator/include"
        "${_sdk}/thirdparty/o1heap/o1heap"
        "${_sdk}/thirdparty/renderdoc"
        "${_sdk}/thirdparty/imgui"
        "${_sdk}/thirdparty/tomlplusplus/include"
        "${_sdk}/thirdparty/spirv-tools/include")
    foreach(_directory IN LISTS _sdk_includes)
        if(NOT IS_DIRECTORY "${_directory}")
            message(FATAL_ERROR "Native integration include directory is missing: ${_directory}")
        endif()
    endforeach()
endif()

add_library(brawlers_recompiled OBJECT ${GENERATED_SOURCES})
add_executable(battle-brawlers
    "${PROJECT_SOURCE_DIR}/src/runtime/native_host.cpp"
    "${PROJECT_SOURCE_DIR}/src/runtime/native_ui.cpp"
    "${PROJECT_SOURCE_DIR}/src/runtime/settings_runtime.cpp"
    "${PROJECT_SOURCE_DIR}/src/game/native_timing.cpp"
    "${PROJECT_SOURCE_DIR}/src/game/native_dialogs.cpp"
    "${PROJECT_SOURCE_DIR}/src/game/native_game_ui.cpp"
    "${PROJECT_SOURCE_DIR}/src/game/native_battle_speech.cpp"
    "${PROJECT_SOURCE_DIR}/src/game/native_versus_speech.cpp"
    "${PROJECT_SOURCE_DIR}/src/game/native_vs_mode.cpp"
    "${PROJECT_SOURCE_DIR}/src/game/native_vs_rules.cpp"
    "${PROJECT_SOURCE_DIR}/src/game/native_vs_arena.cpp"
    "${PROJECT_SOURCE_DIR}/src/game/native_vs_viewer.cpp"
    "${PROJECT_SOURCE_DIR}/src/game/native_vs_characters.cpp"
    "${PROJECT_SOURCE_DIR}/src/game/native_vs_characters_speech.cpp"
    "${PROJECT_SOURCE_DIR}/src/game/native_vs_decks.cpp"
    "${PROJECT_SOURCE_DIR}/src/game/native_vs_editor.cpp"
    "${PROJECT_SOURCE_DIR}/src/game/native_vs_editor_speech.cpp"
    "${PROJECT_SOURCE_DIR}/src/game/native_settings.cpp"
    "${PROJECT_SOURCE_DIR}/src/game/native_settings_save.cpp"
    "${PROJECT_SOURCE_DIR}/src/game/native_settings_render.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/game_speech_queue.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/settings_menu.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/settings_catalog.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/settings_events.cpp")
if(WIN32)
    set_target_properties(battle-brawlers PROPERTIES WIN32_EXECUTABLE ON)
    target_link_libraries(brawlers_recompiled PRIVATE rex::runtime)
    target_link_libraries(battle-brawlers PRIVATE brawlers_recompiled rex::runtime)
    rexglue_apply_target_settings(brawlers_recompiled)
    rexglue_configure_target(battle-brawlers GPU_PLUGINS xenos)
    # The upstream translation template requires SEH for guest exceptions,
    # including when Clang creates the translation's precompiled header.
    target_compile_options(brawlers_recompiled PRIVATE -fasync-exceptions)
else()
    target_sources(battle-brawlers PRIVATE
        "${_sdk}/src/ui/rex_app.cpp"
        "${_sdk}/src/ui/windowed_app_main_sdl.cpp")
endif()

if(BRAWLERS_STEAM_AUDIO_SDK_DIR)
    file(REAL_PATH "${BRAWLERS_STEAM_AUDIO_SDK_DIR}" _steam_audio)
    cmake_path(IS_PREFIX _brawlers_source_root "${_steam_audio}" NORMALIZE _brawlers_in_tree)
    if(_brawlers_in_tree OR NOT EXISTS "${_steam_audio}/include/phonon.h")
        message(FATAL_ERROR "BRAWLERS_STEAM_AUDIO_SDK_DIR must contain external Steam Audio SDK headers")
    endif()
    target_sources(battle-brawlers PRIVATE
        "${PROJECT_SOURCE_DIR}/src/game/native_gate_audio.cpp"
        "${PROJECT_SOURCE_DIR}/src/core/spatial_audio.cpp"
        "${PROJECT_SOURCE_DIR}/src/runtime/spatial_audio_runtime.cpp"
        "${PROJECT_SOURCE_DIR}/src/runtime/accessibility_audio.cpp")
    target_include_directories(battle-brawlers PRIVATE "${_steam_audio}/include")
    target_compile_definitions(battle-brawlers PRIVATE BRAWLERS_SPATIAL_AUDIO=1)
endif()

foreach(_target IN ITEMS brawlers_recompiled battle-brawlers)
    set_target_properties(${_target} PROPERTIES
        CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS ON)
    target_include_directories(${_target} PRIVATE "${_generated}" ${_sdk_includes})
    target_compile_definitions(${_target} PRIVATE
        REX_HAS_VULKAN=1 SPDLOG_COMPILED_LIB SPDLOG_FMT_EXTERNAL NDEBUG)
    if(NOT WIN32)
        target_compile_definitions(${_target} PRIVATE REXGLUE_BUILD_CONFIG="Release" REX_PLATFORM_LINUX=1)
        target_compile_options(${_target} PRIVATE -mcmodel=large)
    endif()
    target_compile_options(${_target} PRIVATE
        -march=x86-64-v2 -fno-strict-aliasing
        -ffp-model=strict -fno-char8_t -gline-tables-only -msse4.1)
endforeach()
# Preserve the verified split: optimized CPU translation, diagnostic host code.
target_compile_options(brawlers_recompiled PRIVATE -O3)
target_compile_options(battle-brawlers PRIVATE -O0)
target_precompile_headers(brawlers_recompiled PRIVATE "${_generated}/battle_brawlers_pch.h")
target_include_directories(battle-brawlers PRIVATE
    "${PROJECT_SOURCE_DIR}/src/core"
    "${PROJECT_SOURCE_DIR}/src/game"
    "${_sdk}/thirdparty/inja/third_party/include")
set_target_properties(battle-brawlers PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/native")
if(NOT WIN32)
    target_link_libraries(battle-brawlers PRIVATE brawlers_recompiled
        "${_sdk_output}/librexruntime.so"
        "${_sdk_output}/libfmt.a"
        "${_sdk_output}/libspdlog.a"
        "${_sdk_output}/libSDL3.a" pthread dl)
    target_link_options(battle-brawlers PRIVATE -Wl,--no-relax)
    set_target_properties(battle-brawlers PROPERTIES
        BUILD_WITH_INSTALL_RPATH ON INSTALL_RPATH "$ORIGIN")
    add_custom_command(TARGET battle-brawlers POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${_sdk_output}/librexruntime.so" "${_sdk_output}/librexgpu-xenos.so"
            "$<TARGET_FILE_DIR:battle-brawlers>"
        VERBATIM)
endif()
message(STATUS "Native integration enabled; no game execution target is provided")
