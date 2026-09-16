#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

namespace onyverb::dsp
{

/** Simple one-pole low-pass, used for HF damping in the feedback path. */
class OnePoleLowpass
{
public:
    void prepare (double sr) { sampleRate = sr; reset(); }
    void reset() { state = 0.0f; }

    /** cutoffAmount: 0 = fully open (no damping), 1 = heavily damped. */
    void setDampingAmount (float cutoffAmount)
    {
        cutoffAmount = juce::jlimit (0.0f, 0.999f, cutoffAmount);
        coeff = cutoffAmount;
    }

    inline float process (float x) noexcept
    {
        state = x + coeff * (state - x);
        return state;
    }

private:
    double sampleRate = 44100.0;
    float coeff = 0.0f;
    float state = 0.0f;
};

/** Two cascaded one-pole high-pass stages (-12dB/octave), same reasoning as
    OnePoleLowpassAbs's cascade below — a single one-pole (-6dB/octave) Low
    Cut left too much sub content audible relative to the knob position. */
class OnePoleHighpass
{
public:
    void prepare (double sr) { sampleRate = sr; reset(); }
    void reset() { state1 = 0.0f; prevIn1 = 0.0f; state2 = 0.0f; prevIn2 = 0.0f; }

    void setCutoff (float hz)
    {
        hz = juce::jlimit (1.0f, (float) (sampleRate * 0.49), hz);
        auto x = std::exp (-2.0f * juce::MathConstants<float>::pi * hz / (float) sampleRate);
        coeff = x;
    }

    inline float process (float x) noexcept
    {
        auto y1 = coeff * (state1 + x - prevIn1);
        prevIn1 = x;
        state1 = y1;

        auto y2 = coeff * (state2 + y1 - prevIn2);
        prevIn2 = y1;
        state2 = y2;

        return y2;
    }

private:
    double sampleRate = 44100.0;
    float coeff = 0.0f;
    float state1 = 0.0f;
    float prevIn1 = 0.0f;
    float state2 = 0.0f;
    float prevIn2 = 0.0f;
};

/** Two cascaded one-pole low-pass stages (-12dB/octave) for pre-input tone
    shaping and the tank's High Cut (separate from feedback damping). A
    single one-pole is only -6dB/octave — gentle enough that content an
    octave above the cutoff is barely touched, which read as "High Cut
    doesn't cut enough" since the input stage (and, through it, the early
    reflections) only ever gets one pass of it. Cascading gives a
    noticeably firmer cut at the same cutoff frequency. */
class OnePoleLowpassAbs
{
public:
    void prepare (double sr) { sampleRate = sr; reset(); }
    void reset() { state1 = 0.0f; state2 = 0.0f; }

    void setCutoff (float hz)
    {
        hz = juce::jlimit (20.0f, (float) (sampleRate * 0.49), hz);
        coeff = std::exp (-2.0f * juce::MathConstants<float>::pi * hz / (float) sampleRate);
    }

    inline float process (float x) noexcept
    {
        state1 = x + coeff * (state1 - x);
        state2 = state1 + coeff * (state2 - state1);
        return state2;
    }

private:
    double sampleRate = 44100.0;
    float coeff = 0.0f;
    float state1 = 0.0f;
    float state2 = 0.0f;
};

/** Fractional-delay line with linear interpolation and optional sinusoidal
    modulation of the read pointer. Used both for the FDN tank lines
    (where subtle modulation prevents metallic ringing) and for pre-delay. */
class ModulatedDelayLine
{
public:
    void prepare (double sr, float maxDelayMs)
    {
        sampleRate = sr;
        auto maxSamples = (int) std::ceil (maxDelayMs * 0.001 * sr) + 8;
        buffer.assign ((size_t) juce::jmax (8, maxSamples), 0.0f);
        writePos = 0;
        reset();
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        modPhase = 0.0f;
    }

    void setBaseDelaySamples (float samples)
    {
        baseDelay = juce::jlimit (0.0f, (float) buffer.size() - 4.0f, samples);
    }

    void setModulation (float depthSamples, float rateHz)
    {
        modDepth = depthSamples;
        modIncrement = (float) (rateHz / sampleRate);
    }

    /** Peek the current (modulated) delayed value without advancing the line.
        Advances the modulation LFO phase; call exactly once per sample, paired
        with a single later call to pushSample(). */
    inline float readTap() noexcept
    {
        auto mod = std::sin (modPhase * juce::MathConstants<float>::twoPi) * modDepth;
        modPhase += modIncrement;
        if (modPhase >= 1.0f)
            modPhase -= 1.0f;

        auto delaySamples = juce::jlimit (0.0f, (float) buffer.size() - 4.0f, baseDelay + mod);

        auto readPosF = (float) writePos - delaySamples;
        while (readPosF < 0.0f)
            readPosF += (float) buffer.size();

        auto idx0 = (int) readPosF;
        auto frac = readPosF - (float) idx0;
        auto idx1 = (idx0 + 1) % (int) buffer.size();

        return buffer[(size_t) idx0] + frac * (buffer[(size_t) idx1] - buffer[(size_t) idx0]);
    }

    /** Write the new input sample and advance the write pointer. */
    inline void pushSample (float input) noexcept
    {
        buffer[(size_t) writePos] = input;
        writePos = (writePos + 1) % (int) buffer.size();
    }

    size_t size() const { return buffer.size(); }

private:
    double sampleRate = 44100.0;
    std::vector<float> buffer;
    int writePos = 0;
    float baseDelay = 0.0f;
    float modDepth = 0.0f;
    float modIncrement = 0.0f;
    float modPhase = 0.0f;
};

/** Simple non-modulated delay used for pre-delay and early-reflection taps. */
class SimpleDelayLine
{
public:
    void prepare (double sr, float maxDelayMs)
    {
        auto maxSamples = (int) std::ceil (maxDelayMs * 0.001 * sr) + 4;
        buffer.assign ((size_t) juce::jmax (4, maxSamples), 0.0f);
        writePos = 0;
    }

    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0f); writePos = 0; }

    inline float readAt (float delaySamples) const noexcept
    {
        delaySamples = juce::jlimit (0.0f, (float) buffer.size() - 2.0f, delaySamples);

        // write() is always called before readAt() for a given sample, so
        // writePos already points one slot past the sample just written —
        // reading at "0 samples ago" (delaySamples == 0) needs writePos - 1
        // to land on that sample. Without the -1 here, delaySamples == 0
        // wrapped all the way around to the OLDEST sample in the buffer
        // instead of the newest, i.e. every read was stale by the buffer's
        // full length (~520ms for pre-delay, ~200ms for early reflections)
        // whenever a knob was left at or near 0 — audible as a fixed delay
        // on the entire wet signal, most obvious at 100% wet with no dry
        // signal to mask it.
        auto readPosF = (float) writePos - 1.0f - delaySamples;
        while (readPosF < 0.0f)
            readPosF += (float) buffer.size();

        auto idx0 = (int) readPosF;
        auto frac = readPosF - (float) idx0;
        auto idx1 = (idx0 + 1) % (int) buffer.size();
        return buffer[(size_t) idx0] + frac * (buffer[(size_t) idx1] - buffer[(size_t) idx0]);
    }

    inline void write (float x) noexcept
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) % (int) buffer.size();
    }

    size_t size() const { return buffer.size(); }

private:
    std::vector<float> buffer;
    int writePos = 0;
};

/** Single all-pass diffuser stage (Schroeder all-pass), used in chains to
    smear transients into dense diffusion ahead of the FDN tank. */
class AllpassDiffuser
{
public:
    void prepare (double sr, float delayMs)
    {
        sampleRate = sr;
        delayLine.prepare (sr, delayMs + 1.0f);
        delaySamples = (float) (delayMs * 0.001 * sr);
        delayLine.setBaseDelaySamples (delaySamples);
    }

    void reset() { delayLine.reset(); }

    /** Changes the delay time within the already-allocated buffer — unlike
        prepare(), this never reallocates, so it's safe to call from
        applyMode() while audio is running (mode switches used to call
        prepare() again here, which reallocated the buffer on the audio
        thread — a real-time-safety violation that could cause a dropout/
        glitch right at the moment the reverb Mode changed). Silently
        clamps to whatever the buffer was originally sized for. */
    void retune (float delayMs)
    {
        delaySamples = (float) (delayMs * 0.001 * sampleRate);
        delayLine.setBaseDelaySamples (delaySamples);
    }

    void setCoefficient (float g) { coeff = juce::jlimit (-0.999f, 0.999f, g); }

    /** Classic Schroeder/Freeverb-style all-pass diffuser: one read + one
        write of the internal delay line per sample, unity-magnitude response. */
    inline float process (float x) noexcept
    {
        auto bufout = delayLine.readTap();
        auto y = -x + bufout;
        delayLine.pushSample (x + bufout * coeff);
        return y;
    }

private:
    ModulatedDelayLine delayLine;
    double sampleRate = 44100.0;
    float coeff = 0.5f;
    float delaySamples = 0.0f;
};

inline float dbToGain (float db) { return juce::Decibels::decibelsToGain (db); }

inline float smoothClamp (float x)
{
    // Soft-limit to keep runaway feedback (e.g. freeze + extreme modulation) from
    // producing unbounded output; transparent in normal operating range.
    return std::tanh (x);
}

/** Replaces a NaN/Inf with silence. tanh() above saturates any finite
    value gracefully but — like almost every floating-point operation —
    passes a NaN straight through unchanged, so a NaN that reaches this
    engine from anywhere keeps propagating (and, fed back into a tank
    line, recirculates indefinitely) rather than being caught by the
    soft-limiter. Applied at the two points a bad value could start
    recirculating or reach the output: the tank's own feedback injection
    and the engine's final output samples. */
inline float sanitize (float x) noexcept
{
    return std::isfinite (x) ? x : 0.0f;
}

} // namespace onyverb::dsp
