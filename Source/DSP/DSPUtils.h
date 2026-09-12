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

/** One-pole high-pass (DC / low-cut) filter, smooth cutoff parameter. */
class OnePoleHighpass
{
public:
    void prepare (double sr) { sampleRate = sr; reset(); }
    void reset() { state = 0.0f; prevIn = 0.0f; }

    void setCutoff (float hz)
    {
        hz = juce::jlimit (1.0f, (float) (sampleRate * 0.49), hz);
        auto x = std::exp (-2.0f * juce::MathConstants<float>::pi * hz / (float) sampleRate);
        coeff = x;
    }

    inline float process (float x) noexcept
    {
        auto y = coeff * (state + x - prevIn);
        prevIn = x;
        state = y;
        return y;
    }

private:
    double sampleRate = 44100.0;
    float coeff = 0.0f;
    float state = 0.0f;
    float prevIn = 0.0f;
};

/** Basic one-pole low-pass for pre-input tone shaping (separate from feedback damping). */
class OnePoleLowpassAbs
{
public:
    void prepare (double sr) { sampleRate = sr; reset(); }
    void reset() { state = 0.0f; }

    void setCutoff (float hz)
    {
        hz = juce::jlimit (20.0f, (float) (sampleRate * 0.49), hz);
        coeff = std::exp (-2.0f * juce::MathConstants<float>::pi * hz / (float) sampleRate);
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
        auto readPosF = (float) writePos - delaySamples;
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
        delayLine.prepare (sr, delayMs + 1.0f);
        delaySamples = (float) (delayMs * 0.001 * sr);
        delayLine.setBaseDelaySamples (delaySamples);
    }

    void reset() { delayLine.reset(); }

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

} // namespace onyverb::dsp
