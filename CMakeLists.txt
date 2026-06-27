cmake_minimum_required(VERSION 3.22)

project(NSDucking VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")

option(NSDUCKING_FETCH_JUCE "Download JUCE with FetchContent when JUCE is not already available" ON)

find_package(JUCE CONFIG QUIET)

if(NOT JUCE_FOUND)
    if(NOT NSDUCKING_FETCH_JUCE)
        message(FATAL_ERROR "JUCE was not found. Set JUCE_DIR or enable NSDUCKING_FETCH_JUCE.")
    endif()

    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.14
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(NSDucking
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "nisesimadao"
    BUNDLE_ID "com.nsducking.plugin"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Nsdc
    PLUGIN_CODE Nsdk
    FORMATS VST3 AU
    PRODUCT_NAME "NSDucking"
)

target_sources(NSDucking
    PRIVATE
        Source/CurveLibrary.cpp
        Source/PluginEditor.cpp
        Source/PluginProcessor.cpp
)

target_include_directories(NSDucking
    PRIVATE
        Source
)

target_compile_definitions(NSDucking
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(NSDucking
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ── Smoke test (CurveLibrary only, no JUCE dependency) ───────────────────────
add_executable(NSDucking_SmokeTest
    tests/CurveSmoke.cpp
    Source/CurveLibrary.cpp
)
target_include_directories(NSDucking_SmokeTest PRIVATE Source)
set_target_properties(NSDucking_SmokeTest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
enable_testing()
add_test(NAME SmokeTest COMMAND NSDucking_SmokeTest)
