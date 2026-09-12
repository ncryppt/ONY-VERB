#pragma once

#include <juce_graphics/juce_graphics.h>
#include <cmath>

namespace onyverb::ui
{

namespace detail
{
    inline void addLeafBlade (juce::Path& path, juce::Point<float> base, float angleRad, float length, float width)
    {
        juce::Point<float> dir (std::cos (angleRad), std::sin (angleRad));
        juce::Point<float> perp (-dir.y, dir.x);
        auto tip = base + dir * length;
        auto mid = base + dir * (length * 0.55f);
        auto left = mid + perp * width;
        auto right = mid - perp * width;

        path.startNewSubPath (base);
        path.quadraticTo (left, tip);
        path.quadraticTo (right, base);
        path.closeSubPath();
    }
}

/** A stylised cannabis-leaf silhouette (there's no such glyph in Unicode,
    so this is a small vector path rather than a font character) — seven
    blades converging at a base point, longest pointing straight up. `size`
    is roughly the tip-to-base length of the longest blade. Centred so the
    base point sits at the path's local origin. */
inline juce::Path makeLeafPath (float size)
{
    juce::Path path;
    juce::Point<float> base (0.0f, 0.0f);

    struct Blade { float angleDeg, lengthMul, widthMul; };
    static const Blade blades[] = {
        { -90.0f, 1.00f, 0.16f },
        { -55.0f, 0.86f, 0.17f }, { -125.0f, 0.86f, 0.17f },
        { -20.0f, 0.64f, 0.18f }, { -160.0f, 0.64f, 0.18f },
        {  18.0f, 0.40f, 0.16f }, { -198.0f, 0.40f, 0.16f },
    };

    for (auto& b : blades)
        detail::addLeafBlade (path, base, juce::degreesToRadians (b.angleDeg), size * b.lengthMul, size * b.widthMul);

    return path;
}

/** A single, rounded leaf — the kind seen on hybrid/EV "eco" badges — rather
    than the multi-bladed cannabis leaf above. One symmetric pointed-oval
    blade from a base point (bottom) to a tip (top), centred on the local
    origin, `size` tall. Pair with makeEcoLeafVeinPath() for the midrib. */
inline juce::Path makeEcoLeafPath (float size)
{
    juce::Path path;
    juce::Point<float> base (0.0f, size * 0.5f);
    juce::Point<float> tip (0.0f, -size * 0.5f);

    path.startNewSubPath (base);
    path.cubicTo (base.x - size * 0.5f, base.y - size * 0.2f,
                  tip.x - size * 0.34f, tip.y + size * 0.4f,
                  tip.x, tip.y);
    path.cubicTo (tip.x + size * 0.34f, tip.y + size * 0.4f,
                  base.x + size * 0.5f, base.y - size * 0.2f,
                  base.x, base.y);
    path.closeSubPath();

    return path;
}

/** The straight midrib vein down the centre of makeEcoLeafPath(), meant to
    be stroked on top of the filled leaf in a contrasting colour. */
inline juce::Path makeEcoLeafVeinPath (float size)
{
    juce::Path path;
    path.startNewSubPath (0.0f, size * 0.42f);
    path.lineTo (0.0f, -size * 0.4f);
    return path;
}

} // namespace onyverb::ui
