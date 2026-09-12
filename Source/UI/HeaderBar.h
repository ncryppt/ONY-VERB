#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "LeafShape.h"
#include "../Parameters.h"
#include "BinaryData.h"

namespace onyverb::ui
{

/** Top header: ONYVA logo, Bypass toggle (bound to the real bypass
    parameter), and A/B compare buttons. A/B swaps the full plugin state
    between two in-memory snapshots — real compare, not decorative — via
    the callback the editor supplies. */
class HeaderBar final : public juce::Component
{
public:
    explicit HeaderBar (juce::AudioProcessorValueTreeState& apvts)
        : bypassAttachment (*apvts.getParameter (ParamIDs::bypass), bypassButton, apvts.undoManager)
    {
        logoImageWhite = juce::ImageCache::getFromMemory (BinaryData::ONYVA_Logowhite_png, BinaryData::ONYVA_Logowhite_pngSize);
        logoImageBlack = juce::ImageCache::getFromMemory (BinaryData::ONYVA_Logoblack_png, BinaryData::ONYVA_Logoblack_pngSize);

        bypassButton.setButtonText ("Bypass");
        bypassButton.getProperties().set ("pill", true);
        bypassButton.setClickingTogglesState (true);
        addAndMakeVisible (bypassButton);
        bypassAttachment.sendInitialUpdate();

        for (auto* b : { &buttonA, &buttonB })
        {
            b->getProperties().set ("pill", true);
            b->setClickingTogglesState (false);
            addAndMakeVisible (*b);
        }
        buttonA.setButtonText ("A");
        buttonB.setButtonText ("B");
        buttonA.setToggleState (true, juce::dontSendNotification);
        buttonA.onClick = [this] { setActiveSlot ('A'); };
        buttonB.onClick = [this] { setActiveSlot ('B'); };
    }

    /** Called by the editor once construction is complete, so HeaderBar
        doesn't need to know how state save/restore works. */
    void onCompareRequested (std::function<void (char slot)> callback) { onCompare = std::move (callback); }

    void refreshTheme() { repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto& logoImage = Theme::currentThemeIsLight ? logoImageBlack : logoImageWhite;
        if (logoImage.isValid())
        {
            auto logoBounds = getLocalBounds().removeFromLeft (logoArea).toFloat().reduced (0, 10.0f);
            logoBounds.removeFromLeft (logoLeftMargin); // keep the mark off the window edge
            juce::RectanglePlacement placement (juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid);
            auto targetRect = logoBounds.withWidth (juce::jmin (logoBounds.getWidth(), logoBounds.getHeight() * (float) logoImage.getWidth() / (float) logoImage.getHeight()));
            g.drawImage (logoImage, targetRect, placement);

            if (Theme::kushKomaActive)
            {
                auto leafSize = targetRect.getHeight() * 0.6f;
                auto leafCentre = juce::Point<float> (targetRect.getRight() + leafSize * 0.75f, targetRect.getCentreY());
                g.setColour (Theme::accent);
                g.fillPath (makeLeafPath (leafSize), juce::AffineTransform::translation (leafCentre));
            }
        }
    }

    void resized() override
    {
        auto b = getLocalBounds();
        b.removeFromLeft (logoArea + 12);

        auto right = b.removeFromRight (170);
        buttonB.setBounds (right.removeFromRight (44).reduced (2));
        buttonA.setBounds (right.removeFromRight (44).reduced (2));
        right.removeFromRight (8);
        bypassButton.setBounds (right.reduced (2));
    }

private:
    void setActiveSlot (char slot)
    {
        buttonA.setToggleState (slot == 'A', juce::dontSendNotification);
        buttonB.setToggleState (slot == 'B', juce::dontSendNotification);
        if (onCompare) onCompare (slot);
    }

    static constexpr int logoArea = 140;
    static constexpr float logoLeftMargin = 28.0f;

    juce::Image logoImageWhite, logoImageBlack;
    juce::TextButton bypassButton, buttonA, buttonB;
    juce::ButtonParameterAttachment bypassAttachment;
    std::function<void (char)> onCompare;
};

} // namespace onyverb::ui
