#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

namespace onyverb::dsp
{

/** Delay-line-based octave-up pitch shifter for the Shimmer mode's feedback
    path. Uses the classic two-tap overlap technique: two read pointers race
    ahead of the write pointer at 2x speed, offset by half a window and
    cross-faded with a Hann envelope, so each tap's wrap-around discontinuity
    is masked by the other tap being at full volume. Cheap, low-latency
    (window-length only), and good enough for a feedback-path shimmer layer
    where some grain character is expected/desirable. */
class PitchShifterOctaveUp
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        windowSamples = (int) std::round (sr * 0.06); // 60ms window
        buffer.assign ((size_t) windowSamples * 4, 0.0f);
        writePos = 0;
        tapPhase[0] = 0.0f;
        tapPhase[1] = (float) windowSamples * 0.5f;
        window.resize ((size_t) windowSamples);
        for (int i = 0; i < windowSamples; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (windowSamples - 1));
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        tapPhase[0] = 0.0f;
        tapPhase[1] = (float) windowSamples * 0.5f;
    }

    inline float process (float x) noexcept
    {
        buffer[(size_t) writePos] = x;

        float out = 0.0f;
        for (int t = 0; t < 2; ++t)
        {
            // Read position trails the write position by tapPhase samples;
            // advancing the tap at 2x write-rate (ratio - 1 = 1.0 extra) is
            // what produces the +1 octave shift.
            auto readPos = (float) writePos - tapPhase[t];
            while (readPos < 0.0f)
                readPos += (float) buffer.size();

            auto idx0 = (int) readPos % (int) buffer.size();
            auto idx1 = (idx0 + 1) % (int) buffer.size();
            auto frac = readPos - std::floor (readPos);
            auto sample = buffer[(size_t) idx0] + frac * (buffer[(size_t) idx1] - buffer[(size_t) idx0]);

            auto windowIdx = juce::jlimit (0, windowSamples - 1, (int) tapPhase[t]);
            out += sample * window[(size_t) windowIdx];

            tapPhase[t] += 2.0f; // ratio 2.0 => +1 octave
            if (tapPhase[t] >= (float) windowSamples)
                tapPhase[t] -= (float) windowSamples;
        }

        writePos = (writePos + 1) % (int) buffer.size();
        return out;
    }

private:
    double sampleRate = 44100.0;
    int windowSamples = 2048;
    std::vector<float> buffer;
    std::vector<float> window;
    int writePos = 0;
    float tapPhase[2] { 0.0f, 0.0f };
};

} // namespace onyverb::dsp
