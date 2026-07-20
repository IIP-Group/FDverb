cmake_minimum_required(VERSION 3.22)
project(FDverb VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(JUCE)

if(APPLE)
    set(FDVERB_FORMATS AU VST3)
else()
    set(FDVERB_FORMATS VST3)
endif()

juce_add_plugin(FDverb
    COMPANY_NAME "IIP"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Iipc
    PLUGIN_CODE Fdvb
    FORMATS ${FDVERB_FORMATS}
    PRODUCT_NAME "FDverb"
    COPY_PLUGIN_AFTER_BUILD TRUE
)

file(GLOB PRESET_XML_FILES "../presets/generated/*.xml")
juce_add_binary_data(FDverbBinaryData SOURCES
    src/eth_logo.png
    src/FDverb_Logo_White.png
    ${PRESET_XML_FILES}
)

juce_generate_juce_header(FDverb)

file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.h")

target_sources(FDverb PRIVATE ${SOURCES} ${HEADERS})

source_group("Source Files" FILES ${SOURCES})
source_group("Header Files" FILES ${HEADERS})

option(FDVERB_MEASURE_TIMING "Log processFrame() wall-clock times to ~/Desktop/fdverb_runtime_measurements/" OFF)

if(FDVERB_MEASURE_TIMING)
    set(FDVERB_VERSION_STRING "${PROJECT_VERSION}-timing")
else()
    set(FDVERB_VERSION_STRING "${PROJECT_VERSION}")
endif()

target_compile_definitions(FDverb
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        PLUGIN_VERSION="${FDVERB_VERSION_STRING}"
        $<$<BOOL:${FDVERB_MEASURE_TIMING}>:FDVERB_MEASURE_TIMING>)

target_link_libraries(FDverb
    PRIVATE
        FDverbBinaryData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

target_include_directories(FDverb PRIVATE
    $<TARGET_PROPERTY:FDverbBinaryData,INTERFACE_INCLUDE_DIRECTORIES>)
