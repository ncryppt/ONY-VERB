#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "DSP/FDNReverb.h"

namespace onyverb
{

class OnyVerbProcessor final : public juce::AudioProcessor
{
public:
    OnyVerbProcessor();
    ~OnyVerbProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 60.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    /** UI reads this on a repaint timer; the audio thread pushes one entry
        per processed block. See Source/DSP/VisualizationData.h. */
    dsp::VisualizationRingBuffer& getVisualizationRingBuffer() noexcept { return visualizationRing; }

private:
    dsp::VisualizationRingBuffer visualizationRing;
    static BusesProperties makeBusesProperties();
    void updateEngineParameters();

    dsp::FDNReverbEngine engine;

    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* sizeParam = nullptr;
    std::atomic<float>* decayParam = nullptr;
    std::atomic<float>* freezeParam = nullptr;
    std::atomic<float>* preDelayParam = nullptr;
    std::atomic<float>* diffusionParam = nullptr;
    std::atomic<float>* dampingParam = nullptr;
    std::atomic<float>* lowCutParam = nullptr;
    std::atomic<float>* highCutParam = nullptr;
    std::atomic<float>* widthParam = nullptr;
    std::atomic<float>* modDepthParam = nullptr;
    std::atomic<float>* modRateParam = nullptr;
    std::atomic<float>* earlyLevelParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* inputGainParam = nullptr;
    std::atomic<float>* outputGainParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OnyVerbProcessor)
};

} // namespace onyverb
