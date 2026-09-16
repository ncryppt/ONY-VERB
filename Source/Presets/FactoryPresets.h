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

    /** Groups presets into named packs in the preset browser (e.g. an
        artist series) instead of one flat list — presets sharing a pack
        name are shown together under a single heading, in the order they
        appear here. */
    const char* pack = "Factory";
};

inline const std::vector<FactoryPreset>& getFactoryPresets()
{
    using P = FactoryPresetParam;
    static const std::vector<FactoryPreset> presets = {
        { "Hall - Clean Piano Verb", {
            P{ ParamIDs::mode, 1 }, P{ ParamIDs::size, 0.705f }, P{ ParamIDs::decayTime, 5.84f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 35.8f }, P{ ParamIDs::diffusion, 0.0f },
            P{ ParamIDs::damping, 0.4f }, P{ ParamIDs::lowCut, 62.0f }, P{ ParamIDs::highCut, 20000.0f },
            P{ ParamIDs::width, 0.586f }, P{ ParamIDs::modDepth, 0.3f }, P{ ParamIDs::modRate, 0.35f },
            P{ ParamIDs::earlyLevel, 0.0f }, P{ ParamIDs::dryLevel, 0.0f }, P{ ParamIDs::wetLevel, 0.577f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }, "Stevie Williams" },
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
