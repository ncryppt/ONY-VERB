#pragma once

#include <juce_core/juce_core.h>
#include <vector>

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
};

/** Single-producer/single-consumer lock-free ring buffer: the audio thread
    pushes one snapshot per processed block, the UI thread drains it on a
    repaint timer. Using juce::AbstractFifo keeps this wait-free on both
    sides, which is the actual requirement for a UI that reads live audio
    thread data without risking a glitch. */
class VisualizationRingBuffer
{
public:
    void prepare (int capacity)
    {
        fifo.setTotalSize (juce::jmax (4, capacity));
        buffer.assign ((size_t) fifo.getTotalSize(), {});
    }

    void push (const VisualizationSnapshot& snapshot) noexcept
    {
        int start1, size1, start2, size2;
        fifo.prepareToWrite (1, start1, size1, start2, size2);
        if (size1 > 0) buffer[(size_t) start1] = snapshot;
        else if (size2 > 0) buffer[(size_t) start2] = snapshot;
        fifo.finishedWrite (size1 + size2);
    }

    /** Drains everything currently queued and returns the most recent
        snapshot (or `fallback` if nothing was ready). */
    VisualizationSnapshot popLatest (const VisualizationSnapshot& fallback) noexcept
    {
        auto numReady = fifo.getNumReady();
        if (numReady <= 0)
            return fallback;

        int start1, size1, start2, size2;
        fifo.prepareToRead (numReady, start1, size1, start2, size2);

        VisualizationSnapshot latest = fallback;
        if (size2 > 0) latest = buffer[(size_t) (start2 + size2 - 1)];
        else if (size1 > 0) latest = buffer[(size_t) (start1 + size1 - 1)];

        fifo.finishedRead (size1 + size2);
        return latest;
    }

private:
    juce::AbstractFifo fifo { 4 };
    std::vector<VisualizationSnapshot> buffer;
};

} // namespace onyverb::dsp
