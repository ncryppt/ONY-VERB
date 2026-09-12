#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/ReverbMode.h"

namespace onyverb
{
/** Central registry of parameter IDs and the APVTS layout.
    Keeping IDs as constants avoids typo-driven bugs when wiring UI later. */
namespace ParamIDs
{
    static constexpr auto mode          = "mode";
    static constexpr auto size          = "size";
    static constexpr auto decayTime     = "decayTime";
    static constexpr auto freeze        = "freeze";
    static constexpr auto preDelay      = "preDelay";
    static constexpr auto diffusion     = "diffusion";
    static constexpr auto damping       = "damping";
    static constexpr auto lowCut        = "lowCut";
    static constexpr auto highCut       = "highCut";
    static constexpr auto width         = "width";
    static constexpr auto modDepth      = "modDepth";
    static constexpr auto modRate       = "modRate";
    static constexpr auto earlyLevel    = "earlyLevel";
    static constexpr auto mix           = "mix";
    static constexpr auto inputGain     = "inputGain";
    static constexpr auto outputGain    = "outputGain";
    static constexpr auto bypass        = "bypass";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using Range = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::mode, 1 }, "Mode", getModeNames(), 1));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::size, 1 }, "Size",
        Range { 0.0f, 1.0f, 0.001f }, 0.492f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::decayTime, 1 }, "Decay Time",
        Range { 0.1f, 60.0f, 0.01f, 0.35f }, 2.0f,
        juce::AudioParameterFloatAttributes().withLabel ("s")));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::freeze, 1 }, "Freeze", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::preDelay, 1 }, "Pre-Delay",
        Range { 0.0f, 500.0f, 0.1f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::diffusion, 1 }, "Diffusion",
        Range { 0.0f, 1.0f, 0.001f }, 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::damping, 1 }, "Damping",
        Range { 0.0f, 1.0f, 0.001f }, 0.4f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::lowCut, 1 }, "Low Cut",
        Range { 20.0f, 2000.0f, 1.0f, 0.3f }, 210.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::highCut, 1 }, "High Cut",
        Range { 200.0f, 20000.0f, 1.0f, 0.3f }, 2224.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::width, 1 }, "Width",
        Range { 0.0f, 1.0f, 0.001f }, 1.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::modDepth, 1 }, "Mod Depth",
        Range { 0.0f, 1.0f, 0.001f }, 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::modRate, 1 }, "Mod Rate",
        Range { 0.02f, 5.0f, 0.001f, 0.5f }, 0.02f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::earlyLevel, 1 }, "Early Reflections",
        Range { 0.0f, 1.0f, 0.001f }, 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::mix, 1 }, "Mix",
        Range { 0.0f, 1.0f, 0.001f }, 0.175f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::inputGain, 1 }, "Input Gain",
        Range { -24.0f, 24.0f, 0.01f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::outputGain, 1 }, "Output Gain",
        Range { -24.0f, 24.0f, 0.01f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::bypass, 1 }, "Bypass", false));

    return { params.begin(), params.end() };
}

} // namespace onyverb
