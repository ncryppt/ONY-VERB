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
        for (auto& f : highCutFilters)
            f.prepare (sr);
        shimmerShifter.prepare (sr);
        reset();
    }

    void reset()
    {
        for (auto& line : lines) line.reset();
        for (auto& f : dampingFilters) f.reset();
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

    void setDecayTime (float seconds)
    {
        decaySeconds = juce::jmax (0.05f, seconds);
        updateLineGains();
    }

    void setFreeze (bool shouldFreeze) { freeze = shouldFreeze; }

    void setDamping (float damping01)
    {
        auto amount = juce::jlimit (0.0f, 0.995f, damping01 + current.dampingBias);
        for (auto& f : dampingFilters)
            f.setDampingAmount (amount);
        updateLineGains();
    }

    // There is deliberately no in-loop Low Cut: the input stage already
    // high-passes everything entering the tank (and the early reflections),
    // so a second high-pass inside the feedback loop only ever added loss
    // that compounds on every recirculation — at the default 210Hz it cut
    // the 250Hz band's decay to a third of the set time. See FDNReverb.h.

    void setHighCutHz (float hz)
    {
        for (auto& f : highCutFilters) f.setCutoff (hz);
        updateLineGains();
    }

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

    void setShimmerAmount (float amount01)
    {
        shimmerAmountRequested = amount01;
        updateLineGains();
    }

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
            v = highCutFilters[(size_t) i].process (v);

            filtered[(size_t) i] = v * (freeze ? 0.9999f : lineGain[(size_t) i]);
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

        updateLineGains();
    }

    /** |H| of the one-pole low-pass y = x + c*(y_prev - x) at angular
        frequency w (radians/sample). Unity at DC. */
    static float onePoleMagnitude (float c, float w)
    {
        auto denom = 1.0f - 2.0f * c * std::cos (w) + c * c;
        return (1.0f - c) / std::sqrt (juce::jmax (1.0e-12f, denom));
    }

    /** Per-line feedback gain for a given decay time.

        The textbook FDN gain g = 10^(-3*d/T) (d = the line's delay, T = the
        wanted RT60) gives exactly T for a loop with no other losses. But
        the damping and High Cut filters sit inside the loop too, so every
        recirculation also loses |H(f)| of those filters on top of g — and
        because that extra loss is a fixed number of dB *per pass*, its
        effect on the decay time grows the longer the decay is. Measured on
        the real engine at the old defaults, a Hall set to 8s decayed in
        about 2.9s, and a Room set to 8s in about 1.8s.

        So g is divided by the filters' combined gain at a reference
        frequency in the middle of the audible range, which makes Decay
        mean "RT60 at roughly 0.5-1kHz" (the usual convention), with the
        filters then shortening the decay above that point exactly as
        Damping/High Cut are meant to. Capped just under unity so that, with
        the orthogonal feedback matrix, the loop can never gain energy. */
    void updateLineGains()
    {
        constexpr float referenceHz = 700.0f;
        constexpr float maxLoopGain = 0.9996f;

        auto w = 2.0f * juce::MathConstants<float>::pi * referenceHz / (float) sampleRate;
        auto referenceLoss = onePoleMagnitude (dampingFilters[0].getCoefficient(), w)
                           * onePoleMagnitude (highCutFilters[0].getCoefficient(), w);
        auto compensation = 1.0f / juce::jmax (0.05f, referenceLoss);

        // The compensation above is relative, so at long decays (where the
        // natural per-pass loss is tiny) it can lift the loop's DC gain far
        // enough that the bass rings much longer than the set time. Real
        // rooms do ring longer in the bass, but not by 2x — so low
        // frequencies are held to at most this multiple of the set decay.
        constexpr float maxLowFrequencyStretch = 1.3f;

        float highestGain = 0.0f;

        for (int i = 0; i < kNumTankLines; ++i)
        {
            auto delaySeconds = lineDelaySamples[(size_t) i] / (float) sampleRate;
            auto natural = std::pow (10.0f, -3.0f * delaySeconds / decaySeconds);
            auto stretchLimit = std::pow (natural, 1.0f / maxLowFrequencyStretch);
            lineGain[(size_t) i] = juce::jmin (maxLoopGain, juce::jmin (natural * compensation, stretchLimit));
            highestGain = juce::jmax (highestGain, lineGain[(size_t) i]);
        }

        // Shimmer feeds the octave-shifted tail back into every line, which
        // adds roughly amount/sqrt(N) to the loop's peak gain on top of the
        // lines' own. With the loop gain now sitting closer to unity at long
        // decays (previously the in-loop filters ate the margin), that sum
        // can pass 1 and the tail grows instead of decaying — so the amount
        // is trimmed to leave headroom. It only bites at long decays.
        auto headroom = juce::jmax (0.0f, 0.995f - highestGain);
        shimmerAmount = juce::jmin (shimmerAmountRequested, headroom * std::sqrt ((float) kNumTankLines));
    }

    double sampleRate = 44100.0;
    std::array<ModulatedDelayLine, kNumTankLines> lines;
    std::array<OnePoleLowpass, kNumTankLines> dampingFilters;
    std::array<OnePoleLowpassGentle, kNumTankLines> highCutFilters; // gentler than the input stage's — see its class comment
    std::array<float, kNumTankLines> lineDelaySamples {};
    std::array<float, kNumTankLines> lineGain {};

    PitchShifterOctaveUp shimmerShifter;
    float shimmerAmountRequested = 0.0f;
    float shimmerAmount = 0.0f; // the requested amount, trimmed for loop stability (see updateLineGains)

    ModeTuning current = getModeTuning (ReverbMode::hall);
    float sizeParam = 0.5f;
    float decaySeconds = 2.0f;
    bool freeze = false;
};

} // namespace onyverb::dsp
