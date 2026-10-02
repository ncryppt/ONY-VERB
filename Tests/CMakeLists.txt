# Headless DSP unit tests. Deliberately does not depend on juce_audio_processors
# or the plugin-client wrapper: the engine in Source/DSP is plain C++/JUCE-DSP
# with no plugin-format dependency, so it can be exercised directly and fast.

juce_add_console_app(ONYVerbTests
    PRODUCT_NAME "ONYVerbTests"
)

target_sources(ONYVerbTests PRIVATE
    DSPTests.cpp
)

target_include_directories(ONYVerbTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Source)

target_compile_definitions(ONYVerbTests PRIVATE
    JUCE_STANDALONE_APPLICATION=1
)

target_link_libraries(ONYVerbTests PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

add_test(NAME ONYVerbTests COMMAND ONYVerbTests)

# Offline reverberation-time measurement tool (not part of ctest): prints how
# closely the Decay knob's seconds match the tail the engine actually
# produces, per mode and per octave band. Shares its analysis code with the
# decay-accuracy unit test.
juce_add_console_app(ONYVerbDecayMeasure
    PRODUCT_NAME "ONYVerbDecayMeasure"
)

target_sources(ONYVerbDecayMeasure PRIVATE
    DecayMeasure.cpp
)

target_include_directories(ONYVerbDecayMeasure PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Source)

target_compile_definitions(ONYVerbDecayMeasure PRIVATE
    JUCE_STANDALONE_APPLICATION=1
)

target_link_libraries(ONYVerbDecayMeasure PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
