#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "../Parameters.h"
#include <functional>

namespace onyverb::ui
{

/** Six pill/tab buttons for Room/Hall/Plate/Chamber/Shimmer/Ambient, bound
    directly to the "mode" choice parameter via a generic ParameterAttachment
    (host automation, undo, and other editor instances all stay in sync). */
class ModePillBar final : public juce::Component
{
public:
    explicit ModePillBar (juce::AudioProcessorValueTreeState& apvts)
        : attachment (*apvts.getParameter (ParamIDs::mode),
                       [this] (float v) { updateSelection ((int) v); },
                       apvts.undoManager)
    {
        for (auto& name : getModeNames())
        {
            auto* button = buttons.add (new juce::TextButton (name));
            button->getProperties().set ("pill", true);
            button->setClickingTogglesState (false);
            auto index = buttons.size() - 1;
            button->onClick = [this, index]
            {
                attachment.setValueAsCompleteGesture ((float) index);
            };
            addAndMakeVisible (button);
        }

        attachment.sendInitialUpdate();
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        auto gap = 6;
        auto w = (bounds.getWidth() - gap * (buttons.size() - 1)) / buttons.size();

        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setBounds (bounds.getX() + i * (w + gap), bounds.getY(), w, bounds.getHeight());
    }

    /** Lets the editor wire shared click-feedback (particle bursts) onto
        every pill without this class needing to know anything about that. */
    void forEachButton (const std::function<void (juce::Button&)>& fn)
    {
        for (auto* b : buttons)
            fn (*b);
    }

private:
    void updateSelection (int selectedIndex)
    {
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (i == selectedIndex, juce::dontSendNotification);
    }

    juce::ParameterAttachment attachment;
    juce::OwnedArray<juce::TextButton> buttons;
};

} // namespace onyverb::ui
