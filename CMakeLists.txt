cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET 12.0 CACHE STRING "" FORCE)

project(stereophield VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE
    GIT_TAG 9.0.3
    GIT_SHALLOW TRUE)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2
    GIT_TAG v3.16.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE Catch2)

set(SPH_WARNINGS -Wall -Wextra)

# Definitions every target that touches JUCE must agree on, so that JUCE's
# inline classes have one layout across sph_dsp and its consumers.
set(SPH_JUCE_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# ---------------------------------------------------------------- sph_dsp
# Only our own DSP code is compiled into this library. It sees the headers of
# juce_core, juce_audio_basics and juce_dsp, and passes the modules on to its
# consumers, which compile the JUCE sources exactly once.
file(GLOB SPH_DSP_SOURCES CONFIGURE_DEPENDS src/dsp/*.cpp src/dsp/*.h)
add_library(sph_dsp STATIC ${SPH_DSP_SOURCES})
target_include_directories(sph_dsp PUBLIC src)
foreach(module juce_core juce_audio_basics juce_dsp)
    target_include_directories(sph_dsp PRIVATE
        $<TARGET_PROPERTY:juce::${module},INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(sph_dsp PRIVATE
        $<TARGET_PROPERTY:juce::${module},INTERFACE_COMPILE_DEFINITIONS>)
endforeach()
target_compile_definitions(sph_dsp
    PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
            $<IF:$<CONFIG:Debug>,DEBUG=1;_DEBUG=1,NDEBUG=1;_NDEBUG=1>
    PUBLIC ${SPH_JUCE_DEFS})
target_compile_options(sph_dsp PRIVATE ${SPH_WARNINGS})
target_link_libraries(sph_dsp
    PRIVATE juce::juce_recommended_config_flags
    INTERFACE juce::juce_core juce::juce_audio_basics juce::juce_dsp)

# ------------------------------------------------------- plugin shared code
file(GLOB SPH_PLUGIN_SOURCES CONFIGURE_DEPENDS
    src/plugin/*.cpp src/plugin/*.h src/ui/*.cpp src/ui/*.h)
add_library(sph_plugin_code INTERFACE)
target_sources(sph_plugin_code INTERFACE ${SPH_PLUGIN_SOURCES})
target_link_libraries(sph_plugin_code INTERFACE
    sph_dsp
    juce::juce_core juce::juce_events juce::juce_data_structures
    juce::juce_audio_basics juce::juce_audio_devices juce::juce_audio_formats
    juce::juce_audio_processors juce::juce_audio_utils juce::juce_dsp
    juce::juce_graphics juce::juce_gui_basics juce::juce_gui_extra)

# ------------------------------------------------------------- the plugin
juce_add_plugin(stereophield
    COMPANY_NAME "highda"
    BUNDLE_ID "com.highda.stereophield"
    PLUGIN_MANUFACTURER_CODE Hgda
    PLUGIN_CODE Stph
    FORMATS AU Standalone
    PRODUCT_NAME "stereophield"
    AU_MAIN_TYPE kAudioUnitType_Effect
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)
target_compile_options(stereophield PRIVATE ${SPH_WARNINGS})
target_link_libraries(stereophield
    PRIVATE sph_plugin_code
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# ------------------------------------------------------------------ tests
include(CTest)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

# Measurements shared by the Catch2 tests and sph_measure. The Dsp set needs
# only sph_dsp; the Plugin set needs the full AudioProcessor.
file(GLOB SPH_MEASURE_DSP CONFIGURE_DEPENDS
    tests/common/Measure.cpp tests/common/MeasureDsp*.cpp)
file(GLOB SPH_MEASURE_PLUGIN CONFIGURE_DEPENDS tests/common/MeasurePlugin*.cpp)

juce_add_console_app(sph_tests PRODUCT_NAME "sph_tests")
file(GLOB SPH_UNIT_TESTS CONFIGURE_DEPENDS tests/unit/*.cpp)
target_sources(sph_tests PRIVATE ${SPH_UNIT_TESTS} ${SPH_MEASURE_DSP})
target_include_directories(sph_tests PRIVATE tests)
target_compile_options(sph_tests PRIVATE ${SPH_WARNINGS})
target_link_libraries(sph_tests PRIVATE
    sph_dsp Catch2::Catch2WithMain juce::juce_recommended_config_flags)
catch_discover_tests(sph_tests)

juce_add_console_app(sph_plugin_tests PRODUCT_NAME "sph_plugin_tests")
file(GLOB SPH_PLUGIN_TESTS CONFIGURE_DEPENDS tests/plugin/*.cpp)
target_sources(sph_plugin_tests PRIVATE ${SPH_PLUGIN_TESTS} ${SPH_MEASURE_DSP} ${SPH_MEASURE_PLUGIN})
target_include_directories(sph_plugin_tests PRIVATE tests)
target_compile_definitions(sph_plugin_tests PRIVATE
    SPH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_compile_options(sph_plugin_tests PRIVATE ${SPH_WARNINGS})
target_link_libraries(sph_plugin_tests PRIVATE
    sph_plugin_code Catch2::Catch2WithMain juce::juce_recommended_config_flags)
catch_discover_tests(sph_plugin_tests)

juce_add_console_app(sph_measure PRODUCT_NAME "sph_measure")
target_sources(sph_measure PRIVATE tests/measure/main.cpp ${SPH_MEASURE_DSP} ${SPH_MEASURE_PLUGIN})
target_include_directories(sph_measure PRIVATE tests)
target_compile_definitions(sph_measure PRIVATE SPH_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_compile_options(sph_measure PRIVATE ${SPH_WARNINGS})
target_link_libraries(sph_measure PRIVATE
    sph_plugin_code juce::juce_recommended_config_flags)

foreach(tool sph_tests sph_plugin_tests sph_measure)
    set_target_properties(${tool} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endforeach()
