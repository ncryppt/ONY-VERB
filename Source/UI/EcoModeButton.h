#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "LeafShape.h"

namespace onyverb::ui
{

/** A pill toggle with a small leaf icon beside the label — a single rounded
    "EV/hybrid eco badge" style leaf (makeEcoLeafPath), not the multi-bladed
    cannabis leaf LeafShape.h also provides for the Kush Koma theme. Plain
    juce::Button subclass (rather than TextButton) since we need to paint
    the icon and text together rather than text alone. */
class EcoModeButton final : public juce::Button
{
public:
    EcoModeButton() : juce::Button ("Eco") {}

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
        auto leafSize = bounds.getHeight() * 0.6f;
        auto leafCentre = juce::Point<float> (bounds.getX() + leafSize * 0.75f, bounds.getCentreY());
        auto leafTransform = juce::AffineTransform::rotation (juce::MathConstants<float>::pi * 0.12f).translated (leafCentre);

        g.setColour (contentColour);
        g.fillPath (makeEcoLeafPath (leafSize), leafTransform);

        auto veinColour = getToggleState() ? Theme::panel : Theme::panelRaised;
        g.setColour (veinColour.withAlpha (0.8f));
        g.strokePath (makeEcoLeafVeinPath (leafSize), juce::PathStrokeType (leafSize * 0.06f), leafTransform);

        g.setColour (contentColour);
        g.setFont (Theme::labelFont (13.5f));
        auto textArea = bounds.withTrimmedLeft (leafSize * 1.7f).toNearestInt();
        g.drawFittedText ("Eco", textArea, juce::Justification::centred, 1);
    }
};

} // namespace onyverb::ui
