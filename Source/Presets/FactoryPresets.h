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

/** Factory presets, pending a redo with user-supplied values/names — left
    empty for now rather than removing the preset browser itself, since
    Save As/Import and any user presets should keep working in the
    meantime. */
inline const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets = {};
    return presets;
}

inline void applyFactoryPreset (juce::AudioProcessorValueTreeState& apvts, const FactoryPreset& preset)
{
    for (auto& pv : preset.values)
        if (auto* param = apvts.getParameter (pv.paramID))
            param->setValueNotifyingHost (param->convertTo0to1 (pv.value));
}

} // namespace onyverb
