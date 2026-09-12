cmake_minimum_required(VERSION 3.22)

project(ONYVerb VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Universal binary architectures" FORCE)
endif()

# Statically link the MSVC runtime on Windows so the built plugin doesn't
# depend on the end user having the Visual C++ Redistributable installed —
# without this, the DLL fails to load silently and the plugin just doesn't
# show up in the host's plugin list, which is exactly what happened in beta
# testing before this was added.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_subdirectory(modules/JUCE)

# ---------------------------------------------------------------------------
# Formats: VST3 + Standalone everywhere; AU on macOS. AAX is wired up but
# only actually built when the AAX SDK path is supplied, since the SDK is
# under NDA and can't be vendored here.
# ---------------------------------------------------------------------------
set(ONYVERB_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND ONYVERB_FORMATS AU)
endif()

set(AAX_SDK_PATH "" CACHE PATH "Path to the Avid AAX SDK, enables the AAX target when set")
if(AAX_SDK_PATH)
    list(APPEND ONYVERB_FORMATS AAX)
    juce_set_aax_sdk_path("${AAX_SDK_PATH}")
endif()

juce_add_binary_data(ONYVerbBinaryData SOURCES
    Resources/ONYVA_Logo-white.png
    Resources/ONYVA_Logo-black.png
)

juce_add_plugin(ONYVerb
    COMPANY_NAME "ONYVA"
    PLUGIN_MANUFACTURER_CODE Onyv
    PLUGIN_CODE Onvb
    FORMATS ${ONYVERB_FORMATS}
    PRODUCT_NAME "ONY Verb"
    BUNDLE_ID "com.onyva.onyverb"
    VST3_CATEGORIES "Fx" "Reverb"
    AU_MAIN_TYPE "kAudioUnitType_Effect"
    AAX_CATEGORY "AAX_ePlugInCategory_Reverb"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_AUTO_MANIFEST TRUE
)

target_sources(ONYVerb PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/Parameters.h
    Source/DSP/DSPUtils.h
    Source/DSP/ReverbMode.h
    Source/DSP/ModeDefinitions.h
    Source/DSP/PitchShifter.h
    Source/DSP/EarlyReflections.h
    Source/DSP/FDNTank.h
    Source/DSP/FDNReverb.h
    Source/DSP/VisualizationData.h
    Source/Presets/FactoryPresets.h
    Source/UI/Theme.h
    Source/UI/OnyvaLookAndFeel.h
    Source/UI/OrbVisualizer.h
    Source/UI/DecayCurveDisplay.h
    Source/UI/CorrelationMeter.h
    Source/UI/ModePillBar.h
    Source/UI/HeaderBar.h
    Source/UI/PresetBar.h
    Source/UI/KnobWithLabel.h
    Source/UI/ThemeSwitcher.h
    Source/UI/ParticleOverlay.h
    Source/UI/LeafShape.h
    Source/UI/EcoModeButton.h
    Source/UI/InsaneModeButton.h
)

target_compile_definitions(ONYVerb PUBLIC
    JUCE_ALSA=0
    JUCE_DIRECTSOUND=0
    JUCE_PLUGINHOST_VST3=0
    JUCE_PLUGINHOST_AU=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(ONYVerb PRIVATE
    ONYVerbBinaryData
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

target_include_directories(ONYVerb PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)

enable_testing()
add_subdirectory(Tests)
