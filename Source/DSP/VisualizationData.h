#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <cmath>

namespace onyverb::dsp
{

/** One block's worth of visualization data, computed cheaply inside
    FDNReverbEngine::process() (block-rate RMS/correlation, not a full
    spectral analysis) and handed to the UI thread. */
struct VisualizationSnapshot
{
    float outputLevel = 0.0f;   // RMS of the final (dry+wet mixed) output this block
    float tailEnergy = 0.0f;    // RMS of the wet tank tail alone, pre-mix — drives the orb even through silence
    float correlation = 0.0f;   // -1..1 stereo correlation of the output
    float brightness = 0.5f;    // 0 = dark/warm, 1 = bright — from current tone-shaping params
    float dryLevel = 0.0f;      // RMS of the dry path's contribution to the output (post Dry knob + output gain)
    float wetLevel = 0.0f;      // RMS of the wet path's contribution to the output (post Wet knob + output gain)
};

/** True for any value a real block-rate RMS/correlation snapshot could
    plausibly hold. Used to reject a torn read (see popLatest() below) or a
    genuine but invalid upstream value before it reaches the UI — a NaN or
    huge number here has visibly "exploded" the orb (garbage radius/colour
    math) and triggered a spurious particle burst (read as a huge transient
    jump) on at least one machine, so every field is checked rather than
    just the ones a given caller happens to use. */
inline bool isPlausibleSnapshot (const VisualizationSnapshot& s) noexcept
{
    auto finite = [] (float v) { return std::isfinite (v); };
    return finite (s.outputLevel) && finite (s.tailEnergy) && finite (s.brightness)
        && finite (s.correlation) && finite (s.dryLevel) && finite (s.wetLevel)
        && s.outputLevel >= 0.0f && s.outputLevel < 1000.0f
        && s.tailEnergy  >= 0.0f && s.tailEnergy  < 1000.0f
        && s.dryLevel    >= 0.0f && s.dryLevel    < 1000.0f
        && s.wetLevel    >= 0.0f && s.wetLevel    < 1000.0f
        && s.correlation >= -1.5f && s.correlation <= 1.5f;
}

/** Single-writer (audio thread), multi-reader (any number of independent UI
    timers) publisher of the latest snapshot. A plain FIFO only supports one
    consumer draining it — with the orb, correlation meter, and dry/wet
    meter all wanting the newest value on their own timers, they'd race to
    drain the same queue and starve each other. A seqlock instead just
    always holds the most recent snapshot: the writer is wait-free, and a
    reader retries only in the vanishingly rare case it lands mid-write. */
class VisualizationRingBuffer
{
public:
    void prepare (int /*capacity*/) {} // kept for call-site compatibility; no queue to size any more

    void push (const VisualizationSnapshot& snapshot) noexcept
    {
        auto seq = sequence.load (std::memory_order_relaxed);
        sequence.store (seq + 1, std::memory_order_release); // odd = write in progress
        latest = snapshot;
        sequence.store (seq + 2, std::memory_order_release); // even = stable
    }

    /** Returns the most recently published snapshot, or `fallback` if none
        has been published yet. Safe to call from multiple UI components
        independently — unlike a FIFO drain, reading here never consumes
        the value for other readers. */
    VisualizationSnapshot popLatest (const VisualizationSnapshot& fallback) const noexcept
    {
        for (int attempt = 0; attempt < 4; ++attempt)
        {
            auto seq1 = sequence.load (std::memory_order_acquire);
            if ((seq1 & 1) != 0) continue; // writer mid-update, retry

            VisualizationSnapshot snap = latest;
            auto seq2 = sequence.load (std::memory_order_acquire);
            if (seq1 != seq2) continue; // torn read (write landed mid-copy), retry

            if (seq1 == 0) return fallback;
            if (! isPlausibleSnapshot (snap)) continue; // still garbage despite matching sequence numbers — retry rather than show it
            return snap;
        }
        return fallback;
    }

private:
    VisualizationSnapshot latest {};
    std::atomic<uint32_t> sequence { 0 };
};

} // namespace onyverb::dsp
