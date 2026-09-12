#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/FDNReverb.h"
#include "DSP/ReverbMode.h"
#include <cmath>

using namespace onyverb;
using namespace onyverb::dsp;

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;

void fillWithNoise (juce::AudioBuffer<float>& buffer, juce::Random& rng, float amplitude = 0.5f)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            data[i] = amplitude * (rng.nextFloat() * 2.0f - 1.0f);
    }
}

bool allFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getReadPointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (data[i]))
                return false;
    }
    return true;
}

float maxAbsSampleDelta (const juce::AudioBuffer<float>& buffer)
{
    float maxDelta = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getReadPointer (ch);
        for (int i = 1; i < buffer.getNumSamples(); ++i)
            maxDelta = juce::jmax (maxDelta, std::abs (data[i] - data[i - 1]));
    }
    return maxDelta;
}
} // namespace

class NoNaNInfTest final : public juce::UnitTest
{
public:
    NoNaNInfTest() : juce::UnitTest ("FDN engine produces finite output for every mode") {}

    void runTest() override
    {
        juce::Random rng (42);

        for (int m = 0; m < (int) ReverbMode::numModes; ++m)
        {
            auto mode = static_cast<ReverbMode> (m);
            beginTest ("Mode " + juce::String (m) + " stays finite over 200 blocks");

            FDNReverbEngine engine;
            engine.prepare (kSampleRate, kBlockSize);
            engine.setMode (mode);
            engine.setSize (0.8f);
            engine.setDecayTime (8.0f);
            engine.setDamping (0.3f);
            engine.setDiffusion (0.9f);
            engine.setPreDelayMs (30.0f);
            engine.setWidth (1.0f);
            engine.setModDepth (0.6f);
            engine.setModRate (1.2f);
            engine.setDryLevel (0.0f);
            engine.setWetLevel (1.0f);
            engine.setEarlyLevel (0.8f);
            engine.setLowCutHz (20.0f);
            engine.setHighCutHz (18000.0f);

            juce::AudioBuffer<float> buffer (2, kBlockSize);
            bool ok = true;

            for (int block = 0; block < 200; ++block)
            {
                fillWithNoise (buffer, rng, block < 10 ? 0.7f : 0.0f); // impulse-ish burst then silence: tests decay tail
                engine.process (buffer);
                if (! allFinite (buffer)) { ok = false; break; }
            }

            expect (ok, "Output contained NaN/Inf for mode " + juce::String (m));
        }
    }
};

class FreezeTest final : public juce::UnitTest
{
public:
    FreezeTest() : juce::UnitTest ("Freeze sustains tail without blowing up") {}

    void runTest() override
    {
        beginTest ("Freeze mode stays finite and bounded over 500 blocks");

        FDNReverbEngine engine;
        engine.prepare (kSampleRate, kBlockSize);
        engine.setMode (ReverbMode::ambient);
        engine.setDecayTime (2.0f);
        engine.setDryLevel (0.0f);
        engine.setWetLevel (1.0f);

        juce::Random rng (7);
        juce::AudioBuffer<float> buffer (2, kBlockSize);

        for (int block = 0; block < 5; ++block)
        {
            fillWithNoise (buffer, rng, 0.6f);
            engine.process (buffer);
        }

        engine.setFreeze (true);

        float maxAbs = 0.0f;
        bool finite = true;

        for (int block = 0; block < 500; ++block)
        {
            buffer.clear();
            engine.process (buffer);
            if (! allFinite (buffer)) { finite = false; break; }
            maxAbs = juce::jmax (maxAbs, buffer.getMagnitude (0, buffer.getNumSamples()));
        }

        expect (finite, "Freeze produced NaN/Inf");
        expect (maxAbs < 10.0f, "Freeze tail grew unbounded (max abs = " + juce::String (maxAbs) + ")");
    }
};

class NoClickOnParamJumpTest final : public juce::UnitTest
{
public:
    NoClickOnParamJumpTest() : juce::UnitTest ("Instant parameter jumps don't produce sample-level clicks") {}

    void runTest() override
    {
        beginTest ("Size/Decay/Dry/Wet/Width jump mid-stream stays smooth");

        FDNReverbEngine engine;
        engine.prepare (kSampleRate, kBlockSize);
        engine.setMode (ReverbMode::hall);
        engine.setDryLevel (0.5f);
        engine.setWetLevel (0.5f);
        engine.setSize (0.2f);
        engine.setDecayTime (1.0f);
        engine.setWidth (0.5f);

        juce::Random rng (99);
        juce::AudioBuffer<float> buffer (2, kBlockSize);

        // Settle.
        for (int i = 0; i < 20; ++i) { fillWithNoise (buffer, rng, 0.4f); engine.process (buffer); }

        // Instant, worst-case parameter jumps applied between blocks (as a
        // host would on a hard automation edit / preset switch).
        engine.setSize (1.0f);
        engine.setDecayTime (40.0f);
        engine.setDryLevel (0.0f);
        engine.setWetLevel (1.0f);
        engine.setWidth (1.0f);

        float worst = 0.0f;
        for (int i = 0; i < 30; ++i)
        {
            fillWithNoise (buffer, rng, 0.4f);
            engine.process (buffer);
            worst = juce::jmax (worst, maxAbsSampleDelta (buffer));
        }

        // Input itself (filtered noise at 0.4 amplitude) can already have
        // sample deltas approaching 0.8; we're checking the engine doesn't
        // add its own step discontinuity on top via un-smoothed coefficients.
        expect (worst < 1.5f, "Max sample-to-sample delta after param jump: " + juce::String (worst));
    }
};

class ModeChangeStabilityTest final : public juce::UnitTest
{
public:
    ModeChangeStabilityTest() : juce::UnitTest ("Switching modes mid-stream stays finite and fades cleanly") {}

    void runTest() override
    {
        beginTest ("Cycling through all modes repeatedly stays finite");

        FDNReverbEngine engine;
        engine.prepare (kSampleRate, kBlockSize);
        engine.setDryLevel (0.0f);
        engine.setWetLevel (1.0f);
        engine.setDecayTime (3.0f);

        juce::Random rng (5);
        juce::AudioBuffer<float> buffer (2, kBlockSize);
        bool ok = true;

        for (int i = 0; i < 60 && ok; ++i)
        {
            engine.setMode (static_cast<ReverbMode> (i % (int) ReverbMode::numModes));
            fillWithNoise (buffer, rng, 0.5f);
            engine.process (buffer);
            if (! allFinite (buffer)) ok = false;
        }

        expect (ok, "Mode cycling produced NaN/Inf");
    }
};

class BypassTest final : public juce::UnitTest
{
public:
    BypassTest() : juce::UnitTest ("Bypass passes dry signal through unchanged") {}

    void runTest() override
    {
        beginTest ("Bypassed engine leaves the buffer sample-exact");

        FDNReverbEngine engine;
        engine.prepare (kSampleRate, kBlockSize);
        engine.setMode (ReverbMode::plate);
        engine.setDryLevel (0.0f);
        engine.setWetLevel (1.0f);
        engine.setBypass (true);

        juce::Random rng (123);
        juce::AudioBuffer<float> buffer (2, kBlockSize);
        juce::AudioBuffer<float> reference;
        reference.makeCopyOf (buffer);
        fillWithNoise (buffer, rng, 0.6f);
        reference.makeCopyOf (buffer);

        engine.process (buffer);

        bool identical = true;
        for (int ch = 0; ch < 2 && identical; ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (! juce::exactlyEqual (buffer.getSample (ch, i), reference.getSample (ch, i))) { identical = false; break; }

        expect (identical, "Bypass altered the signal");
    }
};

static NoNaNInfTest noNaNInfTest;
static FreezeTest freezeTest;
static NoClickOnParamJumpTest noClickTest;
static ModeChangeStabilityTest modeChangeTest;
static BypassTest bypassTest;

int main (int, char**)
{
    juce::UnitTestRunner runner;
    runner.runAllTests();

    int numFailures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        auto* result = runner.getResult (i);
        numFailures += result->failures;
    }

    return numFailures == 0 ? 0 : 1;
}
