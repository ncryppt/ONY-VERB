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
        // A general-purpose factory bank covering all six modes — the
        // category/use-case naming (Drum Room, Vocal Plate, Ambient Wash,
        // etc.) is the same common shorthand any reverb's stock library
        // uses (Valhalla VintageVerb, Ableton's own Reverb included), but
        // every value below is tuned from scratch for this engine's own
        // FDN parameters rather than copied from either.
        { "Room - Drum Room", {
            P{ ParamIDs::mode, 0 }, P{ ParamIDs::size, 0.28f }, P{ ParamIDs::decayTime, 0.75f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 3.0f }, P{ ParamIDs::diffusion, 0.15f },
            P{ ParamIDs::damping, 0.55f }, P{ ParamIDs::lowCut, 90.0f }, P{ ParamIDs::highCut, 9500.0f },
            P{ ParamIDs::width, 0.85f }, P{ ParamIDs::modDepth, 0.1f }, P{ ParamIDs::modRate, 0.35f },
            P{ ParamIDs::earlyLevel, 0.55f }, P{ ParamIDs::dryLevel, 1.0f }, P{ ParamIDs::wetLevel, 0.22f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }, "Factory" },

        { "Chamber - Studio Chamber", {
            P{ ParamIDs::mode, 3 }, P{ ParamIDs::size, 0.45f }, P{ ParamIDs::decayTime, 1.6f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 8.0f }, P{ ParamIDs::diffusion, 0.2f },
            P{ ParamIDs::damping, 0.45f }, P{ ParamIDs::lowCut, 120.0f }, P{ ParamIDs::highCut, 11000.0f },
            P{ ParamIDs::width, 0.9f }, P{ ParamIDs::modDepth, 0.2f }, P{ ParamIDs::modRate, 0.3f },
            P{ ParamIDs::earlyLevel, 0.35f }, P{ ParamIDs::dryLevel, 1.0f }, P{ ParamIDs::wetLevel, 0.25f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }, "Factory" },

        { "Plate - Vocal Plate", {
            P{ ParamIDs::mode, 2 }, P{ ParamIDs::size, 0.5f }, P{ ParamIDs::decayTime, 2.2f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 15.0f }, P{ ParamIDs::diffusion, 0.1f },
            P{ ParamIDs::damping, 0.3f }, P{ ParamIDs::lowCut, 180.0f }, P{ ParamIDs::highCut, 13000.0f },
            P{ ParamIDs::width, 1.0f }, P{ ParamIDs::modDepth, 0.15f }, P{ ParamIDs::modRate, 0.3f },
            P{ ParamIDs::earlyLevel, 0.1f }, P{ ParamIDs::dryLevel, 1.0f }, P{ ParamIDs::wetLevel, 0.28f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }, "Factory" },

        { "Plate - Snare Plate", {
            P{ ParamIDs::mode, 2 }, P{ ParamIDs::size, 0.35f }, P{ ParamIDs::decayTime, 1.1f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 0.0f }, P{ ParamIDs::diffusion, 0.05f },
            P{ ParamIDs::damping, 0.35f }, P{ ParamIDs::lowCut, 150.0f }, P{ ParamIDs::highCut, 12000.0f },
            P{ ParamIDs::width, 0.9f }, P{ ParamIDs::modDepth, 0.1f }, P{ ParamIDs::modRate, 0.3f },
            P{ ParamIDs::earlyLevel, 0.2f }, P{ ParamIDs::dryLevel, 1.0f }, P{ ParamIDs::wetLevel, 0.3f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }, "Factory" },

        { "Hall - Concert Hall", {
            P{ ParamIDs::mode, 1 }, P{ ParamIDs::size, 0.8f }, P{ ParamIDs::decayTime, 3.8f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 25.0f }, P{ ParamIDs::diffusion, 0.1f },
            P{ ParamIDs::damping, 0.35f }, P{ ParamIDs::lowCut, 80.0f }, P{ ParamIDs::highCut, 14000.0f },
            P{ ParamIDs::width, 1.0f }, P{ ParamIDs::modDepth, 0.25f }, P{ ParamIDs::modRate, 0.25f },
            P{ ParamIDs::earlyLevel, 0.3f }, P{ ParamIDs::dryLevel, 1.0f }, P{ ParamIDs::wetLevel, 0.24f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }, "Factory" },

        { "Hall - Ambient Hall", {
            P{ ParamIDs::mode, 1 }, P{ ParamIDs::size, 0.9f }, P{ ParamIDs::decayTime, 6.5f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 40.0f }, P{ ParamIDs::diffusion, 0.15f },
            P{ ParamIDs::damping, 0.25f }, P{ ParamIDs::lowCut, 60.0f }, P{ ParamIDs::highCut, 16000.0f },
            P{ ParamIDs::width, 1.0f }, P{ ParamIDs::modDepth, 0.4f }, P{ ParamIDs::modRate, 0.2f },
            P{ ParamIDs::earlyLevel, 0.15f }, P{ ParamIDs::dryLevel, 1.0f }, P{ ParamIDs::wetLevel, 0.3f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }, "Factory" },

        { "Shimmer - Pad Shimmer", {
            P{ ParamIDs::mode, 4 }, P{ ParamIDs::size, 0.7f }, P{ ParamIDs::decayTime, 8.0f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 20.0f }, P{ ParamIDs::diffusion, 0.2f },
            P{ ParamIDs::damping, 0.2f }, P{ ParamIDs::lowCut, 100.0f }, P{ ParamIDs::highCut, 18000.0f },
            P{ ParamIDs::width, 1.0f }, P{ ParamIDs::modDepth, 0.5f }, P{ ParamIDs::modRate, 0.4f },
            P{ ParamIDs::earlyLevel, 0.1f }, P{ ParamIDs::dryLevel, 1.0f }, P{ ParamIDs::wetLevel, 0.35f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }, "Factory" },

        { "Ambient - Ambient Wash", {
            P{ ParamIDs::mode, 5 }, P{ ParamIDs::size, 0.85f }, P{ ParamIDs::decayTime, 12.0f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 50.0f }, P{ ParamIDs::diffusion, 0.25f },
            P{ ParamIDs::damping, 0.15f }, P{ ParamIDs::lowCut, 50.0f }, P{ ParamIDs::highCut, 15000.0f },
            P{ ParamIDs::width, 1.0f }, P{ ParamIDs::modDepth, 0.35f }, P{ ParamIDs::modRate, 0.15f },
            P{ ParamIDs::earlyLevel, 0.05f }, P{ ParamIDs::dryLevel, 1.0f }, P{ ParamIDs::wetLevel, 0.4f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }, "Factory" },

        // Tuned for four-on-the-floor house: short, damped tails on the
        // drum/percussion side so the groove stays tight, bright plates and
        // modulated shimmer for vocal chops and breakdown pads.
        { "Room - House Drum Bus", {
            P{ ParamIDs::mode, 0 }, P{ ParamIDs::size, 0.22f }, P{ ParamIDs::decayTime, 0.45f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 2.0f }, P{ ParamIDs::diffusion, 0.1f },
            P{ ParamIDs::damping, 0.6f }, P{ ParamIDs::lowCut, 150.0f }, P{ ParamIDs::highCut, 8000.0f },
            P{ ParamIDs::width, 0.7f }, P{ ParamIDs::modDepth, 0.05f }, P{ ParamIDs::modRate, 0.3f },
            P{ ParamIDs::earlyLevel, 0.6f }, P{ ParamIDs::dryLevel, 1.0f }, P{ ParamIDs::wetLevel, 0.18f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }, "House" },

        { "Chamber - House Percussion", {
            P{ ParamIDs::mode, 3 }, P{ ParamIDs::size, 0.3f }, P{ ParamIDs::decayTime, 0.6f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 0.0f }, P{ ParamIDs::diffusion, 0.15f },
            P{ ParamIDs::damping, 0.5f }, P{ ParamIDs::lowCut, 200.0f }, P{ ParamIDs::highCut, 10000.0f },
            P{ ParamIDs::width, 0.8f }, P{ ParamIDs::modDepth, 0.1f }, P{ ParamIDs::modRate, 0.3f },
            P{ ParamIDs::earlyLevel, 0.4f }, P{ ParamIDs::dryLevel, 1.0f }, P{ ParamIDs::wetLevel, 0.2f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }, "House" },

        { "Plate - House Vocal Chop", {
            P{ ParamIDs::mode, 2 }, P{ ParamIDs::size, 0.4f }, P{ ParamIDs::decayTime, 1.3f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 10.0f }, P{ ParamIDs::diffusion, 0.1f },
            P{ ParamIDs::damping, 0.25f }, P{ ParamIDs::lowCut, 250.0f }, P{ ParamIDs::highCut, 15000.0f },
            P{ ParamIDs::width, 1.0f }, P{ ParamIDs::modDepth, 0.2f }, P{ ParamIDs::modRate, 0.4f },
            P{ ParamIDs::earlyLevel, 0.15f }, P{ ParamIDs::dryLevel, 1.0f }, P{ ParamIDs::wetLevel, 0.3f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }, "House" },

        { "Hall - House Lead Space", {
            P{ ParamIDs::mode, 1 }, P{ ParamIDs::size, 0.55f }, P{ ParamIDs::decayTime, 1.8f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 12.0f }, P{ ParamIDs::diffusion, 0.15f },
            P{ ParamIDs::damping, 0.35f }, P{ ParamIDs::lowCut, 200.0f }, P{ ParamIDs::highCut, 13000.0f },
            P{ ParamIDs::width, 0.95f }, P{ ParamIDs::modDepth, 0.2f }, P{ ParamIDs::modRate, 0.35f },
            P{ ParamIDs::earlyLevel, 0.2f }, P{ ParamIDs::dryLevel, 1.0f }, P{ ParamIDs::wetLevel, 0.26f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }, "House" },

        { "Shimmer - House Breakdown Pad", {
            P{ ParamIDs::mode, 4 }, P{ ParamIDs::size, 0.75f }, P{ ParamIDs::decayTime, 9.0f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 15.0f }, P{ ParamIDs::diffusion, 0.25f },
            P{ ParamIDs::damping, 0.15f }, P{ ParamIDs::lowCut, 120.0f }, P{ ParamIDs::highCut, 17000.0f },
            P{ ParamIDs::width, 1.0f }, P{ ParamIDs::modDepth, 0.55f }, P{ ParamIDs::modRate, 0.45f },
            P{ ParamIDs::earlyLevel, 0.05f }, P{ ParamIDs::dryLevel, 1.0f }, P{ ParamIDs::wetLevel, 0.4f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }, "House" },

        { "Hall - Clean Piano Verb", {
            P{ ParamIDs::mode, 1 }, P{ ParamIDs::size, 0.705f }, P{ ParamIDs::decayTime, 5.84f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 35.8f }, P{ ParamIDs::diffusion, 0.0f },
            P{ ParamIDs::damping, 0.4f }, P{ ParamIDs::lowCut, 62.0f }, P{ ParamIDs::highCut, 20000.0f },
            P{ ParamIDs::width, 0.586f }, P{ ParamIDs::modDepth, 0.3f }, P{ ParamIDs::modRate, 0.35f },
            P{ ParamIDs::earlyLevel, 0.0f }, P{ ParamIDs::dryLevel, 0.0f }, P{ ParamIDs::wetLevel, 0.577f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }, "Stevie Williams" },

        { "Hall - Vocal Mid's", {
            P{ ParamIDs::mode, 1 }, P{ ParamIDs::size, 0.492f }, P{ ParamIDs::decayTime, 4.0f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 0.0f }, P{ ParamIDs::diffusion, 0.0f },
            P{ ParamIDs::damping, 0.4f }, P{ ParamIDs::lowCut, 307.0f }, P{ ParamIDs::highCut, 2009.0f },
            P{ ParamIDs::width, 1.0f }, P{ ParamIDs::modDepth, 0.3f }, P{ ParamIDs::modRate, 0.35f },
            P{ ParamIDs::earlyLevel, 0.347f }, P{ ParamIDs::dryLevel, 1.0f }, P{ ParamIDs::wetLevel, 0.29f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }, "TheClaw" },

        { "Hall - Drum Bus", {
            P{ ParamIDs::mode, 1 }, P{ ParamIDs::size, 0.430f }, P{ ParamIDs::decayTime, 2.00f },
            P{ ParamIDs::freeze, 0 }, P{ ParamIDs::preDelay, 0.0f }, P{ ParamIDs::diffusion, 0.0f },
            P{ ParamIDs::damping, 0.4f }, P{ ParamIDs::lowCut, 626.0f }, P{ ParamIDs::highCut, 2145.0f },
            P{ ParamIDs::width, 0.688f }, P{ ParamIDs::modDepth, 0.3f }, P{ ParamIDs::modRate, 0.35f },
            P{ ParamIDs::earlyLevel, 0.0f }, P{ ParamIDs::dryLevel, 1.0f }, P{ ParamIDs::wetLevel, 0.208f },
            P{ ParamIDs::inputGain, 0.0f }, P{ ParamIDs::outputGain, 0.0f },
        }, "TheClaw" },
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
