#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

namespace onyverb::dsp
{

/** How many log-spaced frequency points a spectrum is reduced to before
    being handed to the UI — matches DecayCurveDisplay's own frequency axis
    exactly (its freqAt(), 20Hz..20kHz log-spaced) so the dry/wet traces
    line up on the same x positions as the existing decay-time curve. */
static constexpr int kSpectrumPoints = 48;

/** Windowed-FFT spectrum reduced to kSpectrumPoints magnitude values
    (0..1, dB-scaled and smoothed) rather than exposing raw per-bin data —
    this is a visual reference for shaping Low/High Cut against what's
    actually in the signal, not a precision analyzer, so a modest window
    size and no overlap keep it cheap enough to run twice per block (once
    for dry, once for wet) without denting CPU headroom. */
class SpectrumAnalyzer
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        writePos = 0;
        window.fill (0.0f);
        magnitudes.fill (0.0f);

        for (int i = 0; i < kFFTSize; ++i)
            hannWindow[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (kFFTSize - 1));

        auto binHz = (float) sampleRate / (float) kFFTSize;
        for (int i = 0; i < kSpectrumPoints; ++i)
        {
            auto t = (float) i / (float) (kSpectrumPoints - 1);
            auto hz = 20.0f * std::pow (1000.0f, t);
            binForPoint[(size_t) i] = juce::jlimit (1, kFFTSize / 2 - 1, (int) std::round (hz / binHz));
        }
    }

    void reset()
    {
        writePos = 0;
        window.fill (0.0f);
        magnitudes.fill (0.0f);
    }

    /** Feeds one (mono-summed) sample in. Every kFFTSize-th call runs the
        actual transform and updates getMagnitudes() — cheap in between. */
    inline void pushSample (float x) noexcept
    {
        window[(size_t) writePos] = x;
        if (++writePos >= kFFTSize)
        {
            writePos = 0;
            computeSpectrum();
        }
    }

    const std::array<float, kSpectrumPoints>& getMagnitudes() const noexcept { return magnitudes; }

private:
    static constexpr int kFFTOrder = 12; // 4096-sample window
    static constexpr int kFFTSize = 1 << kFFTOrder;

    void computeSpectrum()
    {
        std::array<float, kFFTSize * 2> fftData {};
        for (int i = 0; i < kFFTSize; ++i)
            fftData[(size_t) i] = window[(size_t) i] * hannWindow[(size_t) i];

        fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

        for (int i = 0; i < kSpectrumPoints; ++i)
        {
            auto mag = fftData[(size_t) binForPoint[(size_t) i]] / (float) kFFTSize;
            auto db = juce::Decibels::gainToDecibels (mag, -100.0f);
            auto normalised = juce::jlimit (0.0f, 1.0f, (db + 90.0f) / 90.0f); // -90dB..0dB -> 0..1
            // Light smoothing between successive windows so the trace eases
            // rather than jumping every ~93ms (kFFTSize / sampleRate).
            magnitudes[(size_t) i] += 0.5f * (normalised - magnitudes[(size_t) i]);
        }
    }

    double sampleRate = 44100.0;
    juce::dsp::FFT fft { kFFTOrder };
    std::array<float, kFFTSize> window {};
    std::array<float, kFFTSize> hannWindow {};
    std::array<int, kSpectrumPoints> binForPoint {};
    std::array<float, kSpectrumPoints> magnitudes {};
    int writePos = 0;
};

} // namespace onyverb::dsp
