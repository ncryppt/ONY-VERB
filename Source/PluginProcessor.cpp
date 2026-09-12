#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace onyverb
{

OnyVerbProcessor::BusesProperties OnyVerbProcessor::makeBusesProperties()
{
    return BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
}

OnyVerbProcessor::OnyVerbProcessor()
    : juce::AudioProcessor (makeBusesProperties()),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    modeParam       = apvts.getRawParameterValue (ParamIDs::mode);
    sizeParam       = apvts.getRawParameterValue (ParamIDs::size);
    decayParam      = apvts.getRawParameterValue (ParamIDs::decayTime);
    freezeParam     = apvts.getRawParameterValue (ParamIDs::freeze);
    preDelayParam   = apvts.getRawParameterValue (ParamIDs::preDelay);
    diffusionParam  = apvts.getRawParameterValue (ParamIDs::diffusion);
    dampingParam    = apvts.getRawParameterValue (ParamIDs::damping);
    lowCutParam     = apvts.getRawParameterValue (ParamIDs::lowCut);
    highCutParam    = apvts.getRawParameterValue (ParamIDs::highCut);
    widthParam      = apvts.getRawParameterValue (ParamIDs::width);
    modDepthParam   = apvts.getRawParameterValue (ParamIDs::modDepth);
    modRateParam    = apvts.getRawParameterValue (ParamIDs::modRate);
    earlyLevelParam = apvts.getRawParameterValue (ParamIDs::earlyLevel);
    mixParam        = apvts.getRawParameterValue (ParamIDs::mix);
    inputGainParam  = apvts.getRawParameterValue (ParamIDs::inputGain);
    outputGainParam = apvts.getRawParameterValue (ParamIDs::outputGain);
    bypassParam     = apvts.getRawParameterValue (ParamIDs::bypass);
}

void OnyVerbProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    updateEngineParameters();
    // ~1 second of block-rate snapshots at typical buffer sizes; the UI only
    // ever wants the latest, but this comfortably covers a slow UI thread.
    visualizationRing.prepare (2048);
}

void OnyVerbProcessor::releaseResources() {}

bool OnyVerbProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void OnyVerbProcessor::updateEngineParameters()
{
    engine.setMode (static_cast<ReverbMode> (juce::jlimit (0, (int) ReverbMode::numModes - 1,
                                                             (int) modeParam->load())));
    engine.setSize (sizeParam->load());
    engine.setDecayTime (decayParam->load());
    engine.setFreeze (freezeParam->load() > 0.5f);
    engine.setPreDelayMs (preDelayParam->load());
    engine.setDiffusion (diffusionParam->load());
    engine.setDamping (dampingParam->load());
    engine.setLowCutHz (lowCutParam->load());
    engine.setHighCutHz (highCutParam->load());
    engine.setWidth (widthParam->load());
    engine.setModDepth (modDepthParam->load());
    engine.setModRate (modRateParam->load());
    engine.setEarlyLevel (earlyLevelParam->load());
    engine.setMix (mixParam->load());
    engine.setInputGainDb (inputGainParam->load());
    engine.setOutputGainDb (outputGainParam->load());
    engine.setBypass (bypassParam->load() > 0.5f);
}

void OnyVerbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    updateEngineParameters();

    // Clear any unexpected extra channels; we only process stereo.
    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    engine.process (buffer);
    visualizationRing.push (engine.getLastSnapshot());
}

juce::AudioProcessorEditor* OnyVerbProcessor::createEditor()
{
    return new OnyVerbEditor (*this);
}

void OnyVerbProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
    }
}

void OnyVerbProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

} // namespace onyverb

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new onyverb::OnyVerbProcessor();
}
