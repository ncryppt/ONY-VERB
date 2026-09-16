#pragma once

#include "DSPUtils.h"
#include "ModeDefinitions.h"
#include "PitchShifter.h"
#include <array>
#include <cmath>

namespace onyverb::dsp
{

/** Applies an 8-point Fast Hadamard Transform in place, normalised so the
    matrix is orthogonal (energy-preserving). This is the feedback mixing
    matrix: it's what turns N independent echoes into a smooth, dense tail
    without the metallic comb-filtering a single delay/feedback pair gives. */
inline void hadamard8InPlace (std::array<float, kNumTankLines>& x) noexcept
{
    for (int len = 1; len < kNumTankLines; len <<= 1)
    {
        for (int i = 0; i < kNumTankLines; i += (len << 1))
        {
            for (int j = i; j < i + len; ++j)
            {
                auto a = x[(size_t) j];
                auto b = x[(size_t) (j + len)];
                x[(size_t) j] = a + b;
                x[(size_t) (j + len)] = a - b;
            }
        }
    }
    static const float norm = 1.0f / std::sqrt ((float) kNumTankLines);
    for (auto& v : x)
        v *= norm;
}

/** The Feedback Delay Network tail: N modulated delay lines coupled through
    an orthogonal (Hadamard) feedback matrix, each with its own damping and
    tone-shaping filters in the loop. This is the "algorithmic core" that
    gives the smooth, artifact-free tail — the modulation on each line is
    what stops that tail from ringing metallically. */
class FDNTank
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        for (auto& line : lines)
            line.prepare (sr, 250.0f);
        for (auto& f : dampingFilters)
            f.prepare (sr);
        for (auto& f : lowCutFilters)
            f.prepare (sr);
        for (auto& f : highCutFilters)
            f.prepare (sr);
        shimmerShifter.prepare (sr);
        reset();
    }

    void reset()
    {
        for (auto& line : lines) line.reset();
        for (auto& f : dampingFilters) f.reset();
        for (auto& f : lowCutFilters) f.reset();
        for (auto& f : highCutFilters) f.reset();
        shimmerShifter.reset();
    }

    void setMode (const ModeTuning& tuning)
    {
        current = tuning;
        applyDelayLengths();
    }

    void setSize (float size01)
    {
        sizeParam = size01;
        applyDelayLengths();
    }

    void setDecayTime (float seconds) { decaySeconds = juce::jmax (0.05f, seconds); }
    void setFreeze (bool shouldFreeze) { freeze = shouldFreeze; }

    void setDamping (float damping01)
    {
        auto amount = juce::jlimit (0.0f, 0.995f, damping01 + current.dampingBias);
        for (auto& f : dampingFilters)
            f.setDampingAmount (amount);
    }

    void setLowCutHz (float hz) { for (auto& f : lowCutFilters) f.setCutoff (hz); }
    void setHighCutHz (float hz) { for (auto& f : highCutFilters) f.setCutoff (hz); }

    void setModulation (float depth01, float rateHz)
    {
        auto scaledRate = rateHz * current.modRateScale;
        for (int i = 0; i < kNumTankLines; ++i)
        {
            // Slightly different phase multiplier per line decorrelates the
            // modulation so lines don't beat together audibly.
            auto depthSamples = depth01 * 6.0f * (0.6f + 0.08f * (float) i);
            auto rate = scaledRate * (0.9f + 0.023f * (float) i);
            lines[(size_t) i].setModulation (depthSamples, rate);
        }
    }

    void setShimmerAmount (float amount01) { shimmerAmount = amount01; }

    /** Advances the tank by one sample. Returns the raw per-line tap values
        (pre-filter) so the caller can build a wide, decorrelated stereo
        image from them. */
    inline std::array<float, kNumTankLines> process (float monoInput) noexcept
    {
        std::array<float, kNumTankLines> rawTaps {};
        std::array<float, kNumTankLines> filtered {};

        for (int i = 0; i < kNumTankLines; ++i)
            rawTaps[(size_t) i] = lines[(size_t) i].readTap();

        float shimmerMonoSum = 0.0f;

        for (int i = 0; i < kNumTankLines; ++i)
        {
            auto v = dampingFilters[(size_t) i].process (rawTaps[(size_t) i]);
            v = lowCutFilters[(size_t) i].process (v);
            v = highCutFilters[(size_t) i].process (v);

            auto delaySeconds = lineDelaySamples[(size_t) i] / (float) sampleRate;
            auto gain = freeze ? 0.9999f
                                : std::pow (10.0f, -3.0f * delaySeconds / decaySeconds);
            filtered[(size_t) i] = v * gain;
            shimmerMonoSum += rawTaps[(size_t) i];
        }

        hadamard8InPlace (filtered);

        float shimmerOut = 0.0f;
        if (current.isShimmer && shimmerAmount > 0.0001f)
            shimmerOut = shimmerShifter.process (shimmerMonoSum * (1.0f / (float) kNumTankLines)) * shimmerAmount;

        static const float inputSpread = 1.0f / std::sqrt ((float) kNumTankLines);
        auto newInputBase = freeze ? 0.0f : monoInput * inputSpread;

        for (int i = 0; i < kNumTankLines; ++i)
        {
            auto newVal = newInputBase + filtered[(size_t) i] + shimmerOut * inputSpread;
            newVal = smoothClamp (newVal * 0.2f) * 5.0f; // gentle soft-limit, transparent in normal range
            lines[(size_t) i].pushSample (sanitize (newVal));
        }

        return rawTaps;
    }

private:
    void applyDelayLengths()
    {
        auto sizeScale = (0.4f + sizeParam * 1.6f);
        sizeScale = 1.0f + (sizeScale - 1.0f) * current.sizeToDelayScale;

        for (int i = 0; i < kNumTankLines; ++i)
        {
            auto ms = current.tankDelaysMs[(size_t) i] * sizeScale;
            auto samples = (float) (ms * 0.001 * sampleRate);
            lineDelaySamples[(size_t) i] = samples;
            lines[(size_t) i].setBaseDelaySamples (samples);
        }
    }

    double sampleRate = 44100.0;
    std::array<ModulatedDelayLine, kNumTankLines> lines;
    std::array<OnePoleLowpass, kNumTankLines> dampingFilters;
    std::array<OnePoleHighpass, kNumTankLines> lowCutFilters;
    std::array<OnePoleLowpassAbs, kNumTankLines> highCutFilters;
    std::array<float, kNumTankLines> lineDelaySamples {};

    PitchShifterOctaveUp shimmerShifter;
    float shimmerAmount = 0.0f;

    ModeTuning current = getModeTuning (ReverbMode::hall);
    float sizeParam = 0.5f;
    float decaySeconds = 2.0f;
    bool freeze = false;
};

} // namespace onyverb::dsp
