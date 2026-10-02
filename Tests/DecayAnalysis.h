#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/FDNReverb.h"
#include <array>
#include <cmath>
#include <vector>

/** Offline reverberation-time measurement for the FDN engine: renders an
    impulse response through the real engine, optionally band-filters it,
    and estimates T60 from the Schroeder backward-integrated decay curve
    (the standard ISO 3382 method: fit the -5..-35dB span and extrapolate
    to -60dB). Shared by the decay-accuracy unit test and the standalone
    measurement tool, so both measure exactly the same way. */
namespace decay_analysis
{
using namespace onyverb;
using namespace onyverb::dsp;

/** Every engine parameter that affects the tail, defaulting to the plugin's
    own APVTS defaults (see Parameters.h) so a measurement with a bare
    EngineSettings reflects what a freshly-inserted instance does. */
struct EngineSettings
{
    ReverbMode mode = ReverbMode::hall;
    float size = 0.492f;
    float decaySeconds = 2.0f;
    float diffusion = 0.0f;
    float damping = 0.4f;
    float lowCutHz = 210.0f;
    float highCutHz = 12000.0f;
    float width = 1.0f;
    float modDepth = 0.3f;
    float modRate = 0.35f;
    float earlyLevel = 0.35f;
    float preDelayMs = 0.0f;
};

struct ImpulseResponse
{
    std::vector<float> left, right;
    double sampleRate = 48000.0;
};

inline ImpulseResponse renderImpulseResponse (const EngineSettings& s, double sampleRate, double lengthSeconds)
{
    constexpr int blockSize = 512;

    FDNReverbEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.setMode (s.mode);
    engine.setSize (s.size);
    engine.setDecayTime (s.decaySeconds);
    engine.setDiffusion (s.diffusion);
    engine.setDamping (s.damping);
    engine.setLowCutHz (s.lowCutHz);
    engine.setHighCutHz (s.highCutHz);
    engine.setWidth (s.width);
    engine.setModDepth (s.modDepth);
    engine.setModRate (s.modRate);
    engine.setEarlyLevel (s.earlyLevel);
    engine.setPreDelayMs (s.preDelayMs);
    engine.setDryLevel (0.0f);
    engine.setWetLevel (1.0f);
    engine.setInputGainDb (0.0f);
    engine.setOutputGainDb (0.0f);

    juce::AudioBuffer<float> buffer (2, blockSize);

    // The engine smooths every parameter at block rate starting from zero,
    // so run silence through it until they have all settled.
    for (int i = 0; i < 400; ++i)
    {
        buffer.clear();
        engine.process (buffer);
    }

    ImpulseResponse ir;
    ir.sampleRate = sampleRate;
    auto totalSamples = (int) (lengthSeconds * sampleRate);
    ir.left.reserve ((size_t) totalSamples);
    ir.right.reserve ((size_t) totalSamples);

    for (int done = 0; done < totalSamples; done += blockSize)
    {
        buffer.clear();
        if (done == 0)
        {
            buffer.setSample (0, 0, 0.5f);
            buffer.setSample (1, 0, 0.5f);
        }
        engine.process (buffer);

        for (int n = 0; n < blockSize && done + n < totalSamples; ++n)
        {
            ir.left.push_back (buffer.getSample (0, n));
            ir.right.push_back (buffer.getSample (1, n));
        }
    }

    return ir;
}

/** RBJ constant-0dB-peak band-pass biquad. */
struct Biquad
{
    double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    static Biquad bandPass (double sampleRate, double centreHz, double q)
    {
        Biquad f;
        auto w0 = 2.0 * juce::MathConstants<double>::pi * centreHz / sampleRate;
        auto alpha = std::sin (w0) / (2.0 * q);
        auto a0 = 1.0 + alpha;
        f.b0 = alpha / a0;
        f.b1 = 0.0;
        f.b2 = -alpha / a0;
        f.a1 = -2.0 * std::cos (w0) / a0;
        f.a2 = (1.0 - alpha) / a0;
        return f;
    }

    double process (double x)
    {
        auto y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

/** Filters into one octave band (two cascaded band-pass sections for a
    steeper skirt), or returns the signal untouched if centreHz <= 0. */
inline std::vector<double> bandLimit (const std::vector<float>& x, double sampleRate, double centreHz)
{
    std::vector<double> y (x.begin(), x.end());
    if (centreHz <= 0.0)
        return y;

    auto f1 = Biquad::bandPass (sampleRate, centreHz, 1.414);
    auto f2 = f1;
    for (auto& v : y)
        v = f2.process (f1.process (v));
    return y;
}

struct T60Result
{
    double seconds = 0.0;    // extrapolated to -60dB
    bool valid = false;
    double fitDynamicRangeDb = 0.0; // how deep the fit actually went (35 = full T30)
};

/** Schroeder backward integration, then a least-squares line over -5..-35dB
    (T30), falling back to -5..-25dB (T20) if the response never gets that
    deep, extrapolated to a 60dB decay. */
inline T60Result estimateT60 (const std::vector<double>& ir, double sampleRate)
{
    T60Result result;
    auto n = ir.size();
    if (n < 16)
        return result;

    std::vector<double> edcDb (n);
    double running = 0.0;
    for (size_t i = n; i-- > 0;)
    {
        running += ir[i] * ir[i];
        edcDb[i] = running;
    }

    auto total = edcDb[0];
    if (total <= 0.0)
        return result;

    for (auto& v : edcDb)
        v = 10.0 * std::log10 (juce::jmax (v / total, 1.0e-30));

    auto fit = [&] (double topDb, double bottomDb) -> bool
    {
        size_t first = n, last = n;
        for (size_t i = 0; i < n; ++i)
        {
            if (first == n && edcDb[i] <= topDb)    first = i;
            if (last == n && edcDb[i] <= bottomDb)  { last = i; break; }
        }
        if (first == n || last == n || last <= first + 8)
            return false;

        // Least-squares line through (t, dB) over the fit span.
        double sumT = 0, sumY = 0, sumTT = 0, sumTY = 0;
        auto count = (double) (last - first + 1);
        for (size_t i = first; i <= last; ++i)
        {
            auto t = (double) i / sampleRate;
            sumT += t; sumY += edcDb[i]; sumTT += t * t; sumTY += t * edcDb[i];
        }
        auto denom = count * sumTT - sumT * sumT;
        if (std::abs (denom) < 1.0e-12)
            return false;

        auto slope = (count * sumTY - sumT * sumY) / denom; // dB per second, negative
        if (slope >= 0.0)
            return false;

        result.seconds = -60.0 / slope;
        result.valid = true;
        result.fitDynamicRangeDb = bottomDb - topDb;
        return true;
    };

    if (! fit (-5.0, -35.0))
        fit (-5.0, -25.0);

    return result;
}

/** T60 averaged over both channels, optionally in one octave band. */
inline T60Result measureT60 (const ImpulseResponse& ir, double bandCentreHz = 0.0)
{
    auto l = estimateT60 (bandLimit (ir.left, ir.sampleRate, bandCentreHz), ir.sampleRate);
    auto r = estimateT60 (bandLimit (ir.right, ir.sampleRate, bandCentreHz), ir.sampleRate);

    T60Result out;
    if (l.valid && r.valid)
    {
        out.valid = true;
        out.seconds = 0.5 * (l.seconds + r.seconds);
        out.fitDynamicRangeDb = juce::jmin (l.fitDynamicRangeDb, r.fitDynamicRangeDb);
    }
    return out;
}

} // namespace decay_analysis
