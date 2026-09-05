cmake_minimum_required(VERSION 3.24)
if(CMAKE_HOST_APPLE AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS plugin architectures")
endif()
project(DeepFry VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(CTest)

add_library(DeepFryCodec STATIC Source/JpegCodec.cpp)
target_include_directories(DeepFryCodec PUBLIC Source)
if(BUILD_TESTING)
    add_executable(DeepFryCodecTests Tests/CodecTests.cpp)
    target_link_libraries(DeepFryCodecTests PRIVATE DeepFryCodec)
    add_test(NAME jpeg_codec COMMAND DeepFryCodecTests)
endif()

option(DEEPFRY_BUILD_PLUGIN "Build the JUCE plugin and standalone application" ON)
if(NOT DEEPFRY_BUILD_PLUGIN)
    return()
endif()

include(FetchContent)
# JUCE 8.0.13, pinned to an immutable release commit.
FetchContent_Declare(JUCE
    URL https://codeload.github.com/juce-framework/JUCE/tar.gz/7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2
    URL_HASH SHA256=488c4f6e2c121316d60801842dba87c44e5dbad4e661cb49bf806b49a38b6a5e
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
FetchContent_MakeAvailable(JUCE)

set(DEEPFRY_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND DEEPFRY_FORMATS AU)
endif()

juce_add_plugin(DeepFry
    COMPANY_NAME "JPEG Pedal"
    BUNDLE_ID "audio.jpegpedal.deepfry"
    PLUGIN_MANUFACTURER_CODE Jpeg
    PLUGIN_CODE Dfry
    PRODUCT_NAME "Deep Fry"
    DESCRIPTION "Audio to pixels to JPEG to audio"
    FORMATS ${DEEPFRY_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Deep Fry processes live audio from your selected input.")

target_sources(DeepFry PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(DeepFry PUBLIC Source)
target_compile_definitions(DeepFry PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(DeepFry PRIVATE DeepFryCodec juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

if(APPLE)
    # JUCE signs the VST3 while generating its manifest. Sign the other local
    # development bundles too, including both slices of a universal binary.
    foreach(bundleTarget DeepFry_AU DeepFry_Standalone)
        add_custom_command(TARGET ${bundleTarget} POST_BUILD
            COMMAND /usr/bin/codesign --force --sign - "$<TARGET_BUNDLE_DIR:${bundleTarget}>"
            VERBATIM)
    endforeach()
endif()

if(BUILD_TESTING)
    juce_add_console_app(DeepFryVerify PRODUCT_NAME "Deep Fry Verification")
    target_sources(DeepFryVerify PRIVATE Tests/PluginTests.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(DeepFryVerify PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(DeepFryVerify PRIVATE DeepFryCodec juce::juce_audio_utils
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME plugin_integration COMMAND DeepFryVerify)
endif()
