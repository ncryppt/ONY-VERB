#pragma once

#include "SpectrumAnalyzer.h"
#include <atomic>
#include <cmath>

namespace onyverb::dsp
{

/** The dry and wet spectra for one analysis window, at the same
    kSpectrumPoints frequency positions as DecayCurveDisplay's own curve. */
struct SpectrumSnapshot
{
    std::array<float, kSpectrumPoints> dry {};
    std::array<float, kSpectrumPoints> wet {};
};

/** Same single-writer/multi-reader seqlock as VisualizationRingBuffer (see
    that class for the full reasoning) — kept separate rather than folded
    into VisualizationSnapshot since this payload is much larger and only
    DecayCurveDisplay actually wants it. */
class SpectrumRingBuffer
{
public:
    void push (const SpectrumSnapshot& snapshot) noexcept
    {
        auto seq = sequence.load (std::memory_order_relaxed);
        sequence.store (seq + 1, std::memory_order_release);
        latest = snapshot;
        sequence.store (seq + 2, std::memory_order_release);
    }

    SpectrumSnapshot popLatest (const SpectrumSnapshot& fallback) const noexcept
    {
        for (int attempt = 0; attempt < 4; ++attempt)
        {
            auto seq1 = sequence.load (std::memory_order_acquire);
            if ((seq1 & 1) != 0) continue;

            SpectrumSnapshot snap = latest;
            auto seq2 = sequence.load (std::memory_order_acquire);
            if (seq1 != seq2) continue;

            if (seq1 == 0) return fallback;

            auto plausible = [] (const std::array<float, kSpectrumPoints>& a)
            {
                for (auto v : a)
                    if (! std::isfinite (v) || v < 0.0f || v > 1.0001f)
                        return false;
                return true;
            };
            if (! plausible (snap.dry) || ! plausible (snap.wet)) continue;

            return snap;
        }
        return fallback;
    }

private:
    SpectrumSnapshot latest {};
    std::atomic<uint32_t> sequence { 0 };
};

} // namespace onyverb::dsp
