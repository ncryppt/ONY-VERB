#pragma once

#include "DSPUtils.h"
#include "ModeDefinitions.h"

namespace onyverb::dsp
{

/** Stereo multi-tap early-reflection module, independent of the FDN tail.
    Simulates room geometry as a handful of discrete taps whose spacing and
    gain come from the mode's tuning table, scaled by the Size parameter. */
class EarlyReflections
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        delayL.prepare (sr, 200.0f);
        delayR.prepare (sr, 200.0f);
    }

    void reset() { delayL.reset(); delayR.reset(); }

    void setMode (const ModeTuning& tuning) { current = tuning; }

    void setSize (float size01) { sizeScale = 0.5f + size01; } // 0.5x .. 1.5x spacing

    inline void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        delayL.write (inL);
        delayR.write (inR);

        outL = 0.0f;
        outR = 0.0f;

        for (int i = 0; i < 6; ++i)
        {
            auto timeMs = current.erTapTimesMs[(size_t) i] * sizeScale;
            auto gain = current.erTapGains[(size_t) i];
            auto samplesL = (float) (timeMs * 0.001 * sampleRate);
            auto samplesR = (float) ((timeMs + current.erStereoOffsetMs) * 0.001 * sampleRate);

            outL += delayL.readAt (samplesL) * gain;
            outR += delayR.readAt (samplesR) * gain;
        }
    }

private:
    double sampleRate = 44100.0;
    SimpleDelayLine delayL, delayR;
    ModeTuning current = getModeTuning (ReverbMode::hall);
    float sizeScale = 1.0f;
};

} // namespace onyverb::dsp
