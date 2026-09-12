#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Parameters.h"
#include <vector>

namespace onyverb
{

struct FactoryPresetParam
{
    const char* paramID;
    float value;
};

struct FactoryPreset
{
    const char* name;
    std::vector<FactoryPresetParam> values;
};

/** 12 factory presets, two per mode, each tuned as a distinct starting
    point rather than a token example — Deliverable 5.5 from the brief.
    `mode` is stored as the raw choice index (matches ReverbMode's
    declaration order in DSP/ReverbMode.h); freeze/gain are set explicitly
    in every preset so selecting one is a full, deterministic recall rather
    than leaving stray state from whatever was dialed in before. */
inline const std::vector<FactoryPreset>& getFactoryPresets()
{
    using P = FactoryPresetParam;
    static const std::vector<FactoryPreset> presets = {
        { "Room - Vocal Booth", {
            P{ ParamIDs::mode, 0 }, P{ ParamIDs::size, 0.25f }, P{ ParamIDs::decayTime, 0.6f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 5.0f }, P{ ParamIDs::diffusion, 0.6f },
            P{ ParamIDs::damping, 0.5f }, P{ ParamIDs::lowCut, 120.0f }, P{ ParamIDs::highCut, 9000.0f },
            P{ ParamIDs::width, 0.8f }, P{ ParamIDs::modDepth, 0.2f }, P{ ParamIDs::modRate, 0.3f },
            P{ ParamIDs::earlyLevel, 0.6f }, P{ ParamIDs::mix, 0.22f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }},
        { "Room - Live Drum Room", {
            P{ ParamIDs::mode, 0 }, P{ ParamIDs::size, 0.45f }, P{ ParamIDs::decayTime, 1.1f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 8.0f }, P{ ParamIDs::diffusion, 0.7f },
            P{ ParamIDs::damping, 0.35f }, P{ ParamIDs::lowCut, 80.0f }, P{ ParamIDs::highCut, 11000.0f },
            P{ ParamIDs::width, 1.0f }, P{ ParamIDs::modDepth, 0.25f }, P{ ParamIDs::modRate, 0.4f },
            P{ ParamIDs::earlyLevel, 0.85f }, P{ ParamIDs::mix, 0.3f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }},
        { "Hall - Concert Hall", {
            P{ ParamIDs::mode, 1 }, P{ ParamIDs::size, 0.65f }, P{ ParamIDs::decayTime, 3.2f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 25.0f }, P{ ParamIDs::diffusion, 0.8f },
            P{ ParamIDs::damping, 0.35f }, P{ ParamIDs::lowCut, 40.0f }, P{ ParamIDs::highCut, 13000.0f },
            P{ ParamIDs::width, 1.0f }, P{ ParamIDs::modDepth, 0.3f }, P{ ParamIDs::modRate, 0.25f },
            P{ ParamIDs::earlyLevel, 0.5f }, P{ ParamIDs::mix, 0.35f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }},
        { "Hall - Cathedral", {
            P{ ParamIDs::mode, 1 }, P{ ParamIDs::size, 0.95f }, P{ ParamIDs::decayTime, 7.5f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 60.0f }, P{ ParamIDs::diffusion, 0.85f },
            P{ ParamIDs::damping, 0.25f }, P{ ParamIDs::lowCut, 30.0f }, P{ ParamIDs::highCut, 10000.0f },
            P{ ParamIDs::width, 1.0f }, P{ ParamIDs::modDepth, 0.35f }, P{ ParamIDs::modRate, 0.2f },
            P{ ParamIDs::earlyLevel, 0.4f }, P{ ParamIDs::mix, 0.4f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }},
        { "Plate - Vintage Plate", {
            P{ ParamIDs::mode, 2 }, P{ ParamIDs::size, 0.4f }, P{ ParamIDs::decayTime, 1.8f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 0.0f }, P{ ParamIDs::diffusion, 0.85f },
            P{ ParamIDs::damping, 0.3f }, P{ ParamIDs::lowCut, 100.0f }, P{ ParamIDs::highCut, 15000.0f },
            P{ ParamIDs::width, 0.9f }, P{ ParamIDs::modDepth, 0.2f }, P{ ParamIDs::modRate, 0.5f },
            P{ ParamIDs::earlyLevel, 0.3f }, P{ ParamIDs::mix, 0.28f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }},
        { "Plate - Drum Plate", {
            P{ ParamIDs::mode, 2 }, P{ ParamIDs::size, 0.3f }, P{ ParamIDs::decayTime, 1.0f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 0.0f }, P{ ParamIDs::diffusion, 0.9f },
            P{ ParamIDs::damping, 0.2f }, P{ ParamIDs::lowCut, 150.0f }, P{ ParamIDs::highCut, 16000.0f },
            P{ ParamIDs::width, 0.85f }, P{ ParamIDs::modDepth, 0.15f }, P{ ParamIDs::modRate, 0.6f },
            P{ ParamIDs::earlyLevel, 0.35f }, P{ ParamIDs::mix, 0.25f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }},
        { "Chamber - Warm Chamber", {
            P{ ParamIDs::mode, 3 }, P{ ParamIDs::size, 0.5f }, P{ ParamIDs::decayTime, 2.0f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 12.0f }, P{ ParamIDs::diffusion, 0.65f },
            P{ ParamIDs::damping, 0.55f }, P{ ParamIDs::lowCut, 90.0f }, P{ ParamIDs::highCut, 8000.0f },
            P{ ParamIDs::width, 0.85f }, P{ ParamIDs::modDepth, 0.3f }, P{ ParamIDs::modRate, 0.35f },
            P{ ParamIDs::earlyLevel, 0.55f }, P{ ParamIDs::mix, 0.3f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }},
        { "Chamber - Vocal Chamber", {
            P{ ParamIDs::mode, 3 }, P{ ParamIDs::size, 0.55f }, P{ ParamIDs::decayTime, 1.6f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 15.0f }, P{ ParamIDs::diffusion, 0.6f },
            P{ ParamIDs::damping, 0.5f }, P{ ParamIDs::lowCut, 110.0f }, P{ ParamIDs::highCut, 8500.0f },
            P{ ParamIDs::width, 0.8f }, P{ ParamIDs::modDepth, 0.25f }, P{ ParamIDs::modRate, 0.3f },
            P{ ParamIDs::earlyLevel, 0.5f }, P{ ParamIDs::mix, 0.26f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }},
        { "Shimmer - Ethereal Shimmer", {
            P{ ParamIDs::mode, 4 }, P{ ParamIDs::size, 0.7f }, P{ ParamIDs::decayTime, 6.0f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 30.0f }, P{ ParamIDs::diffusion, 0.75f },
            P{ ParamIDs::damping, 0.2f }, P{ ParamIDs::lowCut, 60.0f }, P{ ParamIDs::highCut, 14000.0f },
            P{ ParamIDs::width, 1.0f }, P{ ParamIDs::modDepth, 0.5f }, P{ ParamIDs::modRate, 0.4f },
            P{ ParamIDs::earlyLevel, 0.3f }, P{ ParamIDs::mix, 0.45f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }},
        { "Shimmer - Ambient Shimmer Pad", {
            P{ ParamIDs::mode, 4 }, P{ ParamIDs::size, 0.85f }, P{ ParamIDs::decayTime, 10.0f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 50.0f }, P{ ParamIDs::diffusion, 0.8f },
            P{ ParamIDs::damping, 0.15f }, P{ ParamIDs::lowCut, 50.0f }, P{ ParamIDs::highCut, 12000.0f },
            P{ ParamIDs::width, 1.0f }, P{ ParamIDs::modDepth, 0.6f }, P{ ParamIDs::modRate, 0.25f },
            P{ ParamIDs::earlyLevel, 0.2f }, P{ ParamIDs::mix, 0.5f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }},
        { "Ambient - Infinite Wash", {
            P{ ParamIDs::mode, 5 }, P{ ParamIDs::size, 0.9f }, P{ ParamIDs::decayTime, 20.0f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 40.0f }, P{ ParamIDs::diffusion, 0.9f },
            P{ ParamIDs::damping, 0.1f }, P{ ParamIDs::lowCut, 40.0f }, P{ ParamIDs::highCut, 9000.0f },
            P{ ParamIDs::width, 1.0f }, P{ ParamIDs::modDepth, 0.4f }, P{ ParamIDs::modRate, 0.15f },
            P{ ParamIDs::earlyLevel, 0.2f }, P{ ParamIDs::mix, 0.55f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }},
        { "Ambient - Frozen Texture", {
            P{ ParamIDs::mode, 5 }, P{ ParamIDs::size, 0.95f }, P{ ParamIDs::decayTime, 30.0f },
            P{ ParamIDs::freeze, 1 }, P{ ParamIDs::preDelay, 20.0f }, P{ ParamIDs::diffusion, 0.9f },
            P{ ParamIDs::damping, 0.05f }, P{ ParamIDs::lowCut, 30.0f }, P{ ParamIDs::highCut, 8000.0f },
            P{ ParamIDs::width, 1.0f }, P{ ParamIDs::modDepth, 0.35f }, P{ ParamIDs::modRate, 0.1f },
            P{ ParamIDs::earlyLevel, 0.15f }, P{ ParamIDs::mix, 0.6f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }},
    };
    return presets;
}

inline void applyFactoryPreset (juce::AudioProcessorValueTreeState& apvts, const FactoryPreset& preset)
{
    for (auto& pv : preset.values)
        if (auto* param = apvts.getParameter (pv.paramID))
            param->setValueNotifyingHost (param->convertTo0to1 (pv.value));
}

} // namespace onyverb
