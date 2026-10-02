#include "DecayAnalysis.h"
#include <cstdio>
#include <cstring>

using namespace decay_analysis;

// Prints measured T60 (as measured/target ratios) for every mode and a
// spread of Decay settings, broadband and per octave band — a way to see
// whether the Decay knob's seconds actually match the tail that comes out.
//
//   ONYVerbDecayMeasure            plugin defaults
//   ONYVerbDecayMeasure --open     Low Cut 20Hz, High Cut 20kHz, Damping 0
//                                  (isolates the raw feedback loop)
// Wet output level (dBFS RMS over 3s) for a noise burst, per mode and across
// Character — for checking that a DSP change doesn't shift how loud the
// reverb is relative to the dry signal.
static void printLevels()
{
    const char* modeNames[] = { "Room", "Hall", "Plate", "Chamber", "Shimmer", "Ambient" };
    const float characters[] = { 0.0f, 0.25f, 0.5f, 1.0f };

    std::printf ("Wet tail RMS level (dBFS, early reflections off) for a 0.25s noise burst; columns = Character 0 / 0.25 / 0.5 / 1\n");
    for (int m = 0; m < (int) ReverbMode::numModes; ++m)
    {
        std::printf ("  %-8s |", modeNames[m]);
        for (auto ch : characters)
        {
            EngineSettings s;
            s.mode = static_cast<ReverbMode> (m);
            s.decaySeconds = 2.0f;
            s.diffusion = ch;
            s.earlyLevel = 0.0f; // tail only: the early reflections bypass the diffusers

            FDNReverbEngine engine;
            engine.prepare (48000.0, 512);
            engine.setMode (s.mode); engine.setSize (s.size); engine.setDecayTime (s.decaySeconds);
            engine.setDiffusion (s.diffusion); engine.setDamping (s.damping);
            engine.setLowCutHz (s.lowCutHz); engine.setHighCutHz (s.highCutHz);
            engine.setWidth (s.width); engine.setModDepth (s.modDepth); engine.setModRate (s.modRate);
            engine.setEarlyLevel (s.earlyLevel); engine.setPreDelayMs (s.preDelayMs);
            engine.setDryLevel (0.0f); engine.setWetLevel (1.0f); engine.setInputGainDb (0.0f); engine.setOutputGainDb (0.0f);

            juce::AudioBuffer<float> buffer (2, 512);
            for (int i = 0; i < 400; ++i) { buffer.clear(); engine.process (buffer); }

            juce::Random rng (7);
            double sum = 0.0; long count = 0;
            for (int block = 0; block < 280; ++block)
            {
                for (int c2 = 0; c2 < 2; ++c2)
                    for (int n = 0; n < 512; ++n)
                        buffer.setSample (c2, n, block < 24 ? 0.25f * (rng.nextFloat() * 2.0f - 1.0f) : 0.0f);
                engine.process (buffer);
                for (int c2 = 0; c2 < 2; ++c2)
                    for (int n = 0; n < 512; ++n) { auto v = buffer.getSample (c2, n); sum += (double) v * v; ++count; }
            }
            std::printf (" %6.1f ", 10.0 * std::log10 (sum / (double) count + 1.0e-20));
        }
        std::printf ("\n");
    }
}

int main (int argc, char** argv)
{
    if (argc > 1 && std::strcmp (argv[1], "--level") == 0)
    {
        printLevels();
        return 0;
    }

    EngineSettings base;
    auto open = argc > 1 && std::strcmp (argv[1], "--open") == 0;
    if (open)
    {
        base.lowCutHz = 20.0f;
        base.highCutHz = 20000.0f;
        base.damping = 0.0f;
    }

    std::printf ("Settings: %s  (size %.3f, damping %.2f, lowcut %.0f, highcut %.0f)\n",
                 open ? "OPEN" : "plugin defaults", base.size, base.damping, base.lowCutHz, base.highCutHz);
    std::printf ("Values are measured T60 / set Decay (1.00 = exact).\n\n");

    const double bands[] = { 0.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0 };
    const float decays[] = { 0.5f, 1.0f, 2.0f, 4.0f, 8.0f };
    const char* modeNames[] = { "Room", "Hall", "Plate", "Chamber", "Shimmer", "Ambient" };

    for (int m = 0; m < (int) ReverbMode::numModes; ++m)
    {
        std::printf ("%s\n  decay |  broad   250    500    1k     2k     4k     8k\n", modeNames[m]);

        for (auto d : decays)
        {
            auto s = base;
            s.mode = static_cast<ReverbMode> (m);
            s.decaySeconds = d;

            auto ir = renderImpulseResponse (s, 48000.0, (double) d * 2.0 + 3.0);
            std::printf ("  %5.1fs |", d);

            for (auto band : bands)
            {
                auto r = measureT60 (ir, band);
                if (r.valid) std::printf (" %5.2f ", r.seconds / (double) d);
                else         std::printf ("   --  ");
            }
            std::printf ("\n");
        }
        std::printf ("\n");
    }

    return 0;
}
