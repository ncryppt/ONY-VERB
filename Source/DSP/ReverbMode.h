#pragma once

#include <juce_core/juce_core.h>

namespace onyverb
{

/** Lives in the DSP layer (not Parameters.h) so the DSP core has zero
    dependency on juce_audio_processors/juce_graphics and stays directly
    unit-testable and usable from the standalone app without the plugin
    wrapper. Parameters.h includes this, rather than the other way round. */
enum class ReverbMode
{
    room = 0,
    hall,
    plate,
    chamber,
    shimmer,
    ambient,
    numModes
};

inline const juce::StringArray& getModeNames()
{
    static const juce::StringArray names { "Room", "Hall", "Plate", "Chamber", "Shimmer", "Ambient" };
    return names;
}

} // namespace onyverb
