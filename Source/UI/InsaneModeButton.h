#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace onyverb::ui
{

/** The classic "bolt" polygon — reads as lightning/intensity/turbo at a
    glance, which fits a mode that exaggerates the particle system rather
    than anything more literal. Self-contained here since nothing else
    currently needs a bolt icon. */
inline juce::Path makeBoltPath (float size)
{
    juce::Path path;
    auto pt = [size] (float x, float y) { return juce::Point<float> (x * size, y * size); };

    path.startNewSubPath (pt (0.15f, -0.5f));
    path.lineTo (pt (-0.32f, 0.05f));
    path.lineTo (pt (-0.02f, 0.05f));
    path.lineTo (pt (-0.15f, 0.5f));
    path.lineTo (pt (0.32f, -0.08f));
    path.lineTo (pt (0.02f, -0.08f));
    path.closeSubPath();

    return path;
}

/** A pill toggle with a small lightning-bolt icon beside the label — same
    construction as EcoModeButton (a plain juce::Button subclass so the
    icon and text paint together), for Insane Mode's exaggerated particles. */
class InsaneModeButton final : public juce::Button
{
public:
    InsaneModeButton() : juce::Button ("Insane") {}

    void paintButton (juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        auto radius = bounds.getHeight() * 0.5f;

        if (getToggleState())
        {
            g.setColour (Theme::accent.withAlpha (0.16f));
            g.fillRoundedRectangle (bounds, radius);
            g.setColour (Theme::accent);
            g.drawRoundedRectangle (bounds, radius, 1.4f);
        }
        else
        {
            g.setColour (isMouseOverButton ? Theme::panelRaised.brighter (0.05f) : Theme::panelRaised);
            g.fillRoundedRectangle (bounds, radius);
            g.setColour (Theme::hairline);
            g.drawRoundedRectangle (bounds, radius, 1.0f);
        }

        if (isButtonDown)
        {
            g.setColour (juce::Colours::black.withAlpha (0.15f));
            g.fillRoundedRectangle (bounds, radius);
        }

        auto contentColour = getToggleState() ? Theme::accent : Theme::textSecondary;
        auto boltSize = bounds.getHeight() * 0.7f;
        auto boltCentre = juce::Point<float> (bounds.getX() + boltSize * 0.6f, bounds.getCentreY());

        g.setColour (contentColour);
        g.fillPath (makeBoltPath (boltSize), juce::AffineTransform::translation (boltCentre));

        g.setFont (Theme::labelFont (13.5f));
        auto textArea = bounds.withTrimmedLeft (boltSize * 1.35f).toNearestInt();
        g.drawFittedText ("Insane", textArea, juce::Justification::centred, 1);
    }
};

} // namespace onyverb::ui
