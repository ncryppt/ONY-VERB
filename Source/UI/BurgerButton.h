#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace onyverb::ui
{

/** A low-key "hamburger" menu icon (three bars, no pill chrome) that opens
    the settings panel. Shows a small accent dot when an update is waiting. */
class BurgerButton final : public juce::Button
{
public:
    BurgerButton() : juce::Button ("Settings")
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("Settings");
    }

    void setBadge (bool shouldShow)
    {
        if (showBadge != shouldShow)
        {
            showBadge = shouldShow;
            repaint();
        }
    }

    void paintButton (juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto icon = juce::Rectangle<float> (18.0f, 14.0f).withCentre (bounds.getCentre());

        g.setColour ((isButtonDown ? Theme::accent : isMouseOverButton ? Theme::textPrimary : Theme::textSecondary));

        for (int i = 0; i < 3; ++i)
        {
            auto y = icon.getY() + (float) i * (icon.getHeight() - 2.0f) * 0.5f;
            g.fillRoundedRectangle (icon.getX(), y, icon.getWidth(), 2.0f, 1.0f);
        }

        if (showBadge)
        {
            auto dot = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ icon.getRight() + 1.0f, icon.getY() - 1.0f });
            g.setColour (Theme::accentGlow);
            g.fillEllipse (dot.expanded (2.0f));
            g.setColour (Theme::accent);
            g.fillEllipse (dot);
        }
    }

private:
    bool showBadge = false;
};

} // namespace onyverb::ui
