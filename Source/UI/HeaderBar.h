#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "LeafShape.h"
#include "../Parameters.h"
#include "BinaryData.h"

namespace onyverb::ui
{

/** Top header: ONYVA logo and Bypass toggle (bound to the real bypass
    parameter). */
class HeaderBar final : public juce::Component
{
public:
    explicit HeaderBar (juce::AudioProcessorValueTreeState& apvts)
        : bypassAttachment (*apvts.getParameter (ParamIDs::bypass), bypassButton, apvts.undoManager)
    {
        // Only the alpha channel of this image is ever used (as a clip
        // mask in paint()) — the logo is tinted with the theme's own
        // accent colour rather than drawn in its original white, so it
        // reads as branded-to-the-theme rather than just light/dark.
        logoImage = juce::ImageCache::getFromMemory (BinaryData::ONYVA_Logowhite_png, BinaryData::ONYVA_Logowhite_pngSize);

        bypassButton.setButtonText ("Bypass");
        bypassButton.getProperties().set ("pill", true);
        bypassButton.setClickingTogglesState (true);
        addAndMakeVisible (bypassButton);
        bypassAttachment.sendInitialUpdate();
    }

    void refreshTheme() { repaint(); }

    void paint (juce::Graphics& g) override
    {
        if (logoImage.isValid())
        {
            auto logoBounds = getLocalBounds().removeFromLeft (logoArea).toFloat().reduced (0, 10.0f);
            logoBounds.removeFromLeft (logoLeftMargin); // keep the mark off the window edge
            juce::RectanglePlacement placement (juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid);
            auto targetRect = logoBounds.withWidth (juce::jmin (logoBounds.getWidth(), logoBounds.getHeight() * (float) logoImage.getWidth() / (float) logoImage.getHeight()));

            // Clip to the logo's shape (its alpha channel) and fill with
            // the theme's accent instead of drawing the image's own
            // colour — this is what makes the mark tint to match whatever
            // theme is active, including tracking Acid Trip's live hue-cycle.
            auto transform = placement.getTransformToFit (logoImage.getBounds().toFloat(), targetRect);
            g.saveState();
            g.reduceClipRegion (logoImage, transform);
            g.setColour (Theme::accent);
            g.fillRect (targetRect);
            g.restoreState();

            if (Theme::kushKomaActive)
            {
                auto leafSize = targetRect.getHeight() * 0.6f;
                auto leafCentre = juce::Point<float> (targetRect.getRight() + leafSize * 0.75f, targetRect.getCentreY() + leafSize * 0.22f);
                g.setColour (Theme::accent);
                g.fillPath (makeLeafPath (leafSize), juce::AffineTransform::translation (leafCentre));
            }
        }
    }

    void resized() override
    {
        auto b = getLocalBounds();
        b.removeFromLeft (logoArea + 12);

        auto right = b.removeFromRight (90);
        bypassButton.setBounds (right.reduced (2));
    }

private:
    static constexpr int logoArea = 140;
    static constexpr float logoLeftMargin = 28.0f;

    juce::Image logoImage;
    juce::TextButton bypassButton;
    juce::ButtonParameterAttachment bypassAttachment;
};

} // namespace onyverb::ui
