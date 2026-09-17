#pragma once

#include <juce_graphics/juce_graphics.h>

namespace onyverb::ui
{

/** A simple, chubby beaver silhouette — round body, small rounded ears, and
    the flat paddle tail that's the character's defining feature — for the
    "Canada Eh?" theme's particle burst. Built from a handful of overlapping
    ellipses (tail, body, head, ears) rather than a single traced outline,
    which reads perfectly well at the small sizes this renders at. Roughly
    centred on the body, tail trailing to the left and head to the right;
    `size` is approximately the nose-to-tail-tip length. */
inline juce::Path makeBeaverPath (float size)
{
    juce::Path path;

    path.addEllipse (-size * 0.66f, -size * 0.08f, size * 0.40f, size * 0.16f); // flat paddle tail
    path.addEllipse (-size * 0.34f, -size * 0.22f, size * 0.60f, size * 0.44f); // body
    path.addEllipse ( size * 0.18f, -size * 0.18f, size * 0.34f, size * 0.32f); // head
    path.addEllipse ( size * 0.24f, -size * 0.30f, size * 0.10f, size * 0.10f); // ear
    path.addEllipse ( size * 0.40f, -size * 0.30f, size * 0.10f, size * 0.10f); // ear

    return path;
}

} // namespace onyverb::ui
