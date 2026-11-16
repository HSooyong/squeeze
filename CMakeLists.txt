cmake_minimum_required(VERSION 3.22)

# macOS builds target Apple silicon and macOS 14.7 unless the cache already says otherwise.
if(APPLE)
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "14.7" CACHE STRING "Minimum macOS version" FORCE)
    endif()
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "macOS architectures" FORCE)
    endif()
endif()

project(SQUEEZE VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# Plugin code SqzE is the four-character code proposed in the build spec and used by the product.
set(SQUEEZE_PLUGIN_CODE SqzE)
set(SQUEEZE_MANUFACTURER_CODE Brfd)

if(APPLE)
    set(SQUEEZE_FORMATS VST3 AU Standalone)
else()
    set(SQUEEZE_FORMATS VST3 Standalone)
endif()

juce_add_plugin(SQUEEZE
    COMPANY_NAME "Bearfield"
    BUNDLE_ID com.bearfield.SQUEEZE
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE
    PLUGIN_MANUFACTURER_CODE ${SQUEEZE_MANUFACTURER_CODE}
    PLUGIN_CODE ${SQUEEZE_PLUGIN_CODE}
    FORMATS ${SQUEEZE_FORMATS}
    PRODUCT_NAME "SQUEEZE"
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Dynamics)

set(SQUEEZE_PLUGIN_SOURCES
    Source/Parameters.cpp
    Source/State.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_sources(SQUEEZE PRIVATE ${SQUEEZE_PLUGIN_SOURCES})

target_include_directories(SQUEEZE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)

add_library(SqueezeCore STATIC
    Source/Core/OptoModel.cpp
    Source/Core/VcaModel.cpp
    Source/Core/FetModel.cpp
    Source/Core/VariMuModel.cpp
    Source/Core/Engine.cpp)

target_include_directories(SqueezeCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core)
target_compile_features(SqueezeCore PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(SqueezeCore PRIVATE /W4)
else()
    target_compile_options(SqueezeCore PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_compile_definitions(SQUEEZE
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SQUEEZE
    PRIVATE
        SqueezeCore
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

juce_add_console_app(squeeze_tests
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE)

target_sources(squeeze_tests
    PRIVATE
        ${SQUEEZE_PLUGIN_SOURCES}
        tests/Check.cpp
        tests/ParamsTest.cpp
        tests/StateTest.cpp
        tests/ProcessTest.cpp
        tests/TestRunner.cpp)

target_include_directories(squeeze_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
        ${CMAKE_CURRENT_SOURCE_DIR}/tests)

target_compile_definitions(squeeze_tests
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(squeeze_tests
    PRIVATE
        SqueezeCore
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

enable_testing()
add_test(NAME params COMMAND squeeze_tests params)
add_test(NAME state COMMAND squeeze_tests state)
add_test(NAME process COMMAND squeeze_tests process)
set_tests_properties(params state process PROPERTIES TIMEOUT 120)
