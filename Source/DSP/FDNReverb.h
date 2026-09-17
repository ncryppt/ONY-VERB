#pragma once

#include "DSPUtils.h"
#include "ModeDefinitions.h"
#include "FDNTank.h"
#include "EarlyReflections.h"
#include "VisualizationData.h"
#include "SpectrumData.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace onyverb::dsp
{

/** Block-rate one-pole parameter smoother. Cheaper than per-sample
    smoothing for parameters that feed filter/delay recalculation (which we
    only want to redo once per block), while still eliminating step
    discontinuities that would otherwise cause zipper noise under automation. */
class BlockSmoother
{
public:
    void reset (float value) { current = value; target = value; }
    void setTarget (float value) { target = value; }
    void setTimeConstant (float blocksFor63pct) { coeff = 1.0f - std::exp (-1.0f / juce::jmax (1.0f, blocksFor63pct)); }

    float tick()
    {
        current += coeff * (target - current);
        return current;
    }

    float get() const { return current; }

private:
    float current = 0.0f;
    float target = 0.0f;
    float coeff = 0.3f;
};

/** Top-level reverb engine: input gain/filtering -> pre-delay -> input
    diffusion -> early reflections (parallel) -> FDN tank -> stereo
    reconstruction -> width -> dry/wet mix -> output gain. Pure DSP, no JUCE
    parameter/plugin dependencies, so it's directly unit-testable and reusable
    from the standalone app. */
class FDNReverbEngine
{
public:
    void prepare (double sampleRateIn, int /*maxBlockSize*/)
    {
        sampleRate = sampleRateIn;

        for (auto& f : inputLowCut) f.prepare (sampleRate);
        for (auto& f : inputHighCut) f.prepare (sampleRate);
        preDelayL.prepare (sampleRate, 520.0f);
        preDelayR.prepare (sampleRate, 520.0f);
        // Sized for the longest diffusion delay across every mode (ambient's
        // 26.7ms) plus margin, so applyMode() can retune() within this
        // buffer on a mode switch instead of reallocating on the audio
        // thread every time the Mode parameter changes.
        for (auto& chain : inputDiffusers)
            for (auto& stage : chain)
                stage.prepare (sampleRate, 30.0f);

        earlyRefl.prepare (sampleRate);
        tank.prepare (sampleRate);
        drySpectrum.prepare (sampleRate);
        wetSpectrum.prepare (sampleRate);

        inputGainSm.setTimeConstant (4.0f);
        outputGainSm.setTimeConstant (4.0f);
        drySm.setTimeConstant (4.0f);
        wetSm.setTimeConstant (4.0f);
        widthSm.setTimeConstant (4.0f);
        sizeSm.setTimeConstant (12.0f);
        decaySm.setTimeConstant (12.0f);
        preDelaySm.setTimeConstant (12.0f);
        diffusionSm.setTimeConstant (12.0f);
        dampingSm.setTimeConstant (12.0f);
        lowCutSm.setTimeConstant (12.0f);
        highCutSm.setTimeConstant (12.0f);
        modDepthSm.setTimeConstant (12.0f);
        modRateSm.setTimeConstant (12.0f);
        earlyLevelSm.setTimeConstant (12.0f);
        modeFadeSm.setTimeConstant (2.0f);

        reset();
        applyMode (currentMode, true);
    }

    void reset()
    {
        for (auto& f : inputLowCut) f.reset();
        for (auto& f : inputHighCut) f.reset();
        preDelayL.reset();
        preDelayR.reset();
        for (auto& chain : inputDiffusers)
            for (auto& stage : chain)
                stage.reset();
        earlyRefl.reset();
        tank.reset();
        drySpectrum.reset();
        wetSpectrum.reset();
    }

    void setMode (ReverbMode mode)
    {
        if (mode != currentMode)
            applyMode (mode, false);
    }

    void setSize (float v)         { sizeSm.setTarget (juce::jlimit (0.0f, 1.0f, v)); }
    void setDecayTime (float s)    { decaySm.setTarget (juce::jlimit (0.1f, 60.0f, s)); }
    void setFreeze (bool f)        { freeze = f; }
    void setPreDelayMs (float ms)  { preDelaySm.setTarget (juce::jlimit (0.0f, 500.0f, ms)); }
    void setDiffusion (float v)    { diffusionSm.setTarget (juce::jlimit (0.0f, 1.0f, v)); }
    void setDamping (float v)      { dampingSm.setTarget (juce::jlimit (0.0f, 1.0f, v)); }
    void setLowCutHz (float hz)    { lowCutSm.setTarget (hz); }
    void setHighCutHz (float hz)   { highCutSm.setTarget (hz); }
    void setWidth (float v)        { widthSm.setTarget (juce::jlimit (0.0f, 1.0f, v)); }
    void setModDepth (float v)     { modDepthSm.setTarget (juce::jlimit (0.0f, 1.0f, v)); }
    void setModRate (float hz)     { modRateSm.setTarget (hz); }
    void setEarlyLevel (float v)   { earlyLevelSm.setTarget (juce::jlimit (0.0f, 1.0f, v)); }
    void setDryLevel (float v)     { drySm.setTarget (juce::jlimit (0.0f, 1.0f, v)); }
    void setWetLevel (float v)     { wetSm.setTarget (juce::jlimit (0.0f, 1.0f, v)); }
    void setInputGainDb (float db) { inputGainSm.setTarget (dbToGain (db)); }
    void setOutputGainDb (float db){ outputGainSm.setTarget (dbToGain (db)); }
    void setBypass (bool b)        { bypassed = b; }

    /** Processes a stereo buffer in place. Any channel-count/layout
        adaptation happens in the processor; this always expects exactly
        two channels of 32-bit float. */
    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        jassert (buffer.getNumChannels() == 2);
        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);
        auto numSamples = buffer.getNumSamples();

        // Block-rate parameter refresh.
        auto sizeV = sizeSm.tick();
        auto decayV = decaySm.tick();
        auto preDelayV = preDelaySm.tick();
        auto diffusionV = diffusionSm.tick();
        auto dampingV = dampingSm.tick();
        auto lowCutV = lowCutSm.tick();
        auto highCutV = highCutSm.tick();
        auto modDepthV = modDepthSm.tick();
        auto modRateV = modRateSm.tick();
        auto earlyLevelV = earlyLevelSm.tick();
        auto modeFadeV = modeFadeSm.tick();

        // Each mode has its own tonal tilt (e.g. Plate brighter, Chamber
        // warmer); the user's High Cut acts as an additional ceiling on top.
        // Applied to the tank itself, not just the input stage — otherwise
        // the tail keeps ringing at the raw (often wide-open) High Cut value
        // regardless of mode, which is a big part of why the default sound
        // was harsher/brighter than intended.
        auto effectiveHighCut = juce::jmin (highCutV, modeTuningRef().toneTiltHz);

        tank.setSize (sizeV);
        tank.setDecayTime (decayV);
        tank.setFreeze (freeze);
        tank.setDamping (dampingV);
        tank.setLowCutHz (lowCutV);
        tank.setHighCutHz (effectiveHighCut);
        tank.setModulation (modDepthV, modRateV);
        tank.setShimmerAmount (modeTuningRef().isShimmer ? 0.6f : 0.0f);
        earlyRefl.setSize (sizeV);

        for (auto& f : inputLowCut) f.setCutoff (lowCutV);
        for (auto& f : inputHighCut) f.setCutoff (effectiveHighCut);

        auto preDelaySamples = (float) (preDelayV * 0.001 * sampleRate);

        // Floor raised from 0.35 so the tail stays reasonably smeared even
        // with Character all the way down, instead of leaning on modulation
        // alone to hide comb-filtering.
        auto diffCoeff = 0.45f + diffusionV * 0.5f;
        for (auto& chain : inputDiffusers)
            for (auto& stage : chain)
                stage.setCoefficient (diffCoeff);

        double sumOutL2 = 0.0, sumOutR2 = 0.0, sumOutLR = 0.0, sumTail2 = 0.0;
        double sumDry2 = 0.0, sumWet2 = 0.0;

        for (int n = 0; n < numSamples; ++n)
        {
            auto inGain = inputGainSm.tick();
            auto outGain = outputGainSm.tick();
            auto dryV = drySm.tick();
            auto wetV = wetSm.tick();
            auto widthV = widthSm.tick();

            auto dryL = left[n];
            auto dryR = right[n];

            if (bypassed)
                continue;

            auto x0 = dryL * inGain;
            auto x1 = dryR * inGain;

            x0 = inputHighCut[0].process (x0);
            x1 = inputHighCut[1].process (x1);
            x0 = inputLowCut[0].process (x0);
            x1 = inputLowCut[1].process (x1);

            preDelayL.write (x0);
            preDelayR.write (x1);
            auto pdL = preDelayL.readAt (preDelaySamples);
            auto pdR = preDelayR.readAt (preDelaySamples);

            auto erInL = pdL, erInR = pdR;
            for (auto& stage : inputDiffusers[0]) pdL = stage.process (pdL);
            for (auto& stage : inputDiffusers[1]) pdR = stage.process (pdR);

            float erL = 0.0f, erR = 0.0f;
            earlyRefl.process (erInL, erInR, erL, erR);

            auto monoIn = (pdL + pdR) * 0.5f;
            auto tapOutputs = tank.process (monoIn);

            // Build two decorrelated-but-balanced combinations of the tank
            // taps for L/R (alternating-sign patterns), then blend toward
            // mono by `width` for mono compatibility.
            float wetL = 0.0f, wetR = 0.0f;
            for (int i = 0; i < kNumTankLines; ++i)
            {
                auto sign = ((i % 3) == 0) ? 1.0f : -1.0f;
                wetL += tapOutputs[(size_t) i] * ((i % 2 == 0) ? 1.0f : sign);
                wetR += tapOutputs[(size_t) i] * ((i % 2 == 0) ? sign : 1.0f);
            }
            static const float tapNorm = 1.0f / std::sqrt ((float) kNumTankLines);
            wetL *= tapNorm;
            wetR *= tapNorm;

            wetL += erL * earlyLevelV;
            wetR += erR * earlyLevelV;

            auto mid = (wetL + wetR) * 0.5f;
            auto side = (wetL - wetR) * 0.5f * widthV;
            wetL = mid + side;
            wetR = mid - side;

            wetL *= modeFadeV;
            wetR *= modeFadeV;

            auto dryOutL = dryL * dryV * outGain;
            auto dryOutR = dryR * dryV * outGain;
            auto wetOutL = wetL * wetV * outGain;
            auto wetOutR = wetR * wetV * outGain;

            auto outL = sanitize (smoothClamp (dryOutL + wetOutL));
            auto outR = sanitize (smoothClamp (dryOutR + wetOutR));
            left[n] = outL;
            right[n] = outR;

            sumOutL2 += (double) outL * outL;
            sumOutR2 += (double) outR * outR;
            sumOutLR += (double) outL * outR;
            sumTail2 += (double) (wetL * wetL + wetR * wetR) * 0.5;
            sumDry2 += (double) (dryOutL * dryOutL + dryOutR * dryOutR) * 0.5;
            sumWet2 += (double) (wetOutL * wetOutL + wetOutR * wetOutR) * 0.5;

            drySpectrum.pushSample ((dryOutL + dryOutR) * 0.5f);
            wetSpectrum.pushSample ((wetOutL + wetOutR) * 0.5f);
        }

        lastSpectrum.dry = drySpectrum.getMagnitudes();
        lastSpectrum.wet = wetSpectrum.getMagnitudes();

        auto invN = numSamples > 0 ? 1.0 / (double) numSamples : 0.0;
        lastSnapshot.outputLevel = (float) std::sqrt (juce::jmax (0.0, (sumOutL2 + sumOutR2) * 0.5 * invN));
        lastSnapshot.tailEnergy = (float) std::sqrt (juce::jmax (0.0, sumTail2 * invN));
        auto denom = std::sqrt (juce::jmax (1.0e-9, sumOutL2 * sumOutR2));
        lastSnapshot.correlation = juce::jlimit (-1.0f, 1.0f, (float) (sumOutLR / denom));
        lastSnapshot.brightness = juce::jlimit (0.0f, 1.0f,
            (highCutV - 2000.0f) / 16000.0f * (1.0f - dampingV));
        lastSnapshot.dryLevel = (float) std::sqrt (juce::jmax (0.0, sumDry2 * invN));
        lastSnapshot.wetLevel = (float) std::sqrt (juce::jmax (0.0, sumWet2 * invN));
    }

    const VisualizationSnapshot& getLastSnapshot() const noexcept { return lastSnapshot; }
    const SpectrumSnapshot& getLastSpectrum() const noexcept { return lastSpectrum; }

private:
    const ModeTuning& modeTuningRef() const { return getModeTuning (currentMode); }

    void applyMode (ReverbMode mode, bool immediate)
    {
        currentMode = mode;
        const auto& tuning = getModeTuning (mode);
        tank.setMode (tuning);
        earlyRefl.setMode (tuning);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 4; ++i)
                inputDiffusers[(size_t) ch][(size_t) i].retune (tuning.diffusionDelaysMs[(size_t) i]);

        if (immediate)
        {
            modeFadeSm.reset (1.0f);
        }
        else
        {
            // Brief mute-and-recover ramp masks the topology swap so mode
            // changes don't produce an audible click/pop in the tail.
            modeFadeSm.reset (0.0f);
            modeFadeSm.setTarget (1.0f);
            tank.reset();
        }
    }

    double sampleRate = 44100.0;
    ReverbMode currentMode = ReverbMode::hall;
    bool freeze = false;
    bool bypassed = false;
    VisualizationSnapshot lastSnapshot;
    SpectrumAnalyzer drySpectrum, wetSpectrum;
    SpectrumSnapshot lastSpectrum;

    std::array<OnePoleHighpass, 2> inputLowCut;   // low-cut = highpass
    std::array<OnePoleLowpassAbs, 2> inputHighCut; // high-cut = lowpass
    SimpleDelayLine preDelayL, preDelayR;
    std::array<std::array<AllpassDiffuser, 4>, 2> inputDiffusers;

    EarlyReflections earlyRefl;
    FDNTank tank;

    BlockSmoother inputGainSm, outputGainSm, drySm, wetSm, widthSm;
    BlockSmoother sizeSm, decaySm, preDelaySm, diffusionSm, dampingSm;
    BlockSmoother lowCutSm, highCutSm, modDepthSm, modRateSm, earlyLevelSm;
    BlockSmoother modeFadeSm;
};

} // namespace onyverb::dsp
