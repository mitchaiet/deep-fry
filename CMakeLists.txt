cmake_minimum_required(VERSION 3.24)
if(CMAKE_HOST_APPLE AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS plugin architectures")
endif()
if(CMAKE_HOST_APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()
project(DeepFry VERSION 0.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    # Keep the Windows download self-contained without a VC runtime installer.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
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

set(DEEPFRY_VST3_AUTO_MANIFEST TRUE)
if(CMAKE_CROSSCOMPILING AND WIN32)
    if(NOT CMAKE_CROSSCOMPILING_EMULATOR)
        message(FATAL_ERROR "Windows cross builds require CMAKE_CROSSCOMPILING_EMULATOR (for example, Wine).")
    endif()
    set(DEEPFRY_VST3_AUTO_MANIFEST FALSE)
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
    VST3_AUTO_MANIFEST ${DEEPFRY_VST3_AUTO_MANIFEST}
    VST3_CATEGORIES Fx Distortion
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Deep Fry processes live audio from your selected input.")

target_sources(DeepFry PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(DeepFry PUBLIC Source)
target_compile_definitions(DeepFry PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    $<$<PLATFORM_ID:Windows>:JUCE_ASIO=0>
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(DeepFry PRIVATE DeepFryCodec juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

if(CMAKE_CROSSCOMPILING AND WIN32)
    # JUCE's ordinary helper launches a target executable directly. Build that
    # same upstream helper with our toolchain and run it through the emulator.
    add_executable(DeepFryCrossVst3ManifestHelper
        "${JUCE_SOURCE_DIR}/modules/juce_audio_plugin_client/VST3/juce_VST3ManifestHelper.cpp")
    target_compile_definitions(DeepFryCrossVst3ManifestHelper PRIVATE
        "$<TARGET_PROPERTY:DeepFry,COMPILE_DEFINITIONS>")
    target_include_directories(DeepFryCrossVst3ManifestHelper PRIVATE
        "$<TARGET_PROPERTY:DeepFry,INCLUDE_DIRECTORIES>")
    add_dependencies(DeepFry_VST3 DeepFryCrossVst3ManifestHelper)
    get_target_property(_deepfry_vst3_bundle DeepFry_VST3 JUCE_PLUGIN_ARTEFACT_FILE)
    add_custom_command(TARGET DeepFry_VST3 POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
            "-DEMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}"
            "-DHELPER_EXE=$<TARGET_FILE:DeepFryCrossVst3ManifestHelper>"
            "-DOUTPUT_FILE=${_deepfry_vst3_bundle}/Contents/Resources/moduleinfo.json"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate-vst3-manifest.cmake"
        VERBATIM)
endif()

if(APPLE)
    # Sign every complete bundle after JUCE's VST3 manifest generation.
    # Signing before moduleinfo.json is written leaves that resource unsealed.
    foreach(bundleTarget DeepFry_VST3 DeepFry_AU DeepFry_Standalone)
        add_custom_command(TARGET ${bundleTarget} POST_BUILD
            COMMAND /usr/bin/codesign --force --sign - "$<TARGET_BUNDLE_DIR:${bundleTarget}>"
            VERBATIM)
    endforeach()
endif()

if(BUILD_TESTING)
    juce_add_console_app(DeepFryVerify PRODUCT_NAME "Deep Fry Verification")
    target_sources(DeepFryVerify PRIVATE Tests/PluginTests.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(DeepFryVerify PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 $<$<PLATFORM_ID:Windows>:JUCE_ASIO=0>)
    target_link_libraries(DeepFryVerify PRIVATE DeepFryCodec juce::juce_audio_utils
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME plugin_integration COMMAND DeepFryVerify)
endif()
