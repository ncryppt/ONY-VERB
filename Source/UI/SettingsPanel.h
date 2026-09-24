#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include <functional>

namespace onyverb::ui
{

/** A plain "x" that closes the settings card — no pill chrome, just two
    crossed strokes that brighten on hover. */
class CloseXButton final : public juce::Button
{
public:
    CloseXButton() : juce::Button ("Close")
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paintButton (juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = getLocalBounds().toFloat();

        if (isMouseOverButton)
        {
            g.setColour (Theme::textPrimary.withAlpha (0.08f));
            g.fillEllipse (bounds);
        }

        auto arm = juce::Rectangle<float> (11.0f, 11.0f).withCentre (bounds.getCentre());
        g.setColour (isButtonDown ? Theme::accent : isMouseOverButton ? Theme::textPrimary : Theme::textSecondary);
        g.drawLine (arm.getX(), arm.getY(), arm.getRight(), arm.getBottom(), 2.0f);
        g.drawLine (arm.getX(), arm.getBottom(), arm.getRight(), arm.getY(), 2.0f);
    }
};

/** Modal-style settings card that dims the plugin behind it. Holds the
    "check for updates" control and its result, and the automatic-check
    toggle. It owns no logic beyond layout — the editor
    wires the callbacks. */
class SettingsPanel final : public juce::Component
{
public:
    std::function<void()> onCheckNow, onDownload, onClose;
    std::function<void (bool)> onAutoCheckChanged;

    explicit SettingsPanel (bool autoCheckEnabled)
    {
        setWantsKeyboardFocus (true);

        addAndMakeVisible (closeButton);

        for (auto* b : { &checkButton, &downloadButton, &autoCheckButton })
        {
            b->getProperties().set ("pill", true);
            addAndMakeVisible (b);
        }

        closeButton.onClick = [this] { if (onClose) onClose(); };

        checkButton.setButtonText ("Check for updates");
        checkButton.onClick = [this] { if (onCheckNow) onCheckNow(); };

        downloadButton.setButtonText ("Download");
        downloadButton.onClick = [this] { if (onDownload) onDownload(); };
        downloadButton.setVisible (false);

        autoCheckButton.setClickingTogglesState (true);
        autoCheckButton.setToggleState (autoCheckEnabled, juce::dontSendNotification);
        autoCheckButton.setButtonText (autoCheckEnabled ? "On" : "Off");
        autoCheckButton.onClick = [this]
        {
            auto on = autoCheckButton.getToggleState();
            autoCheckButton.setButtonText (on ? "On" : "Off");
            if (onAutoCheckChanged) onAutoCheckChanged (on);
        };

        setVisible (false);
    }

    void setUpdateStatus (const juce::String& text, bool showDownload)
    {
        statusText = text;
        downloadButton.setVisible (showDownload);
        resized();
        repaint();
    }

    void setChecking (bool checking) { checkButton.setEnabled (! checking); }

    void open()
    {
        setVisible (true);
        toFront (true);
        grabKeyboardFocus();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.62f));

        auto card = cardBounds.toFloat();
        Theme::fillBeveledRoundedRect (g, card, 14.0f, Theme::panelRaised);

        auto inner = cardBounds.reduced (padding, padding);
        g.setColour (Theme::textPrimary);
        g.setFont (Theme::titleFont (18.0f));
        g.drawText ("SETTINGS", inner.removeFromTop (rowHeight - 6), juce::Justification::centredLeft);

        drawRule (g, inner);
        drawRowLabel (g, inner.removeFromTop (rowHeight), "CHECK FOR UPDATES AUTOMATICALLY");

        drawRule (g, inner);
        inner.removeFromTop (rowHeight); // the "check for updates" button row

        g.setColour (Theme::textSecondary);
        g.setFont (Theme::labelFont (13.0f));
        auto statusRow = inner.removeFromTop (rowHeight);
        g.drawFittedText (statusText, statusRow, juce::Justification::centred, 2);
    }

    void resized() override
    {
        cardBounds = getLocalBounds().withSizeKeepingCentre (juce::jmin (getWidth() - 40, 600), rowHeight * 4 + padding * 2 - 6 + (downloadButton.isVisible() ? rowHeight : 0));

        auto inner = cardBounds.reduced (padding, padding);
        closeButton.setBounds (juce::Rectangle<int> (30, 30).withRightX (cardBounds.getRight() - 14).withY (cardBounds.getY() + 14));

        inner.removeFromTop (rowHeight - 6);                                     // title
        autoCheckButton.setBounds (inner.removeFromTop (rowHeight).removeFromRight (80).reduced (0, 8));
        checkButton.setBounds (inner.removeFromTop (rowHeight).withSizeKeepingCentre (210, rowHeight - 16));

        // Status text takes the row below; when an update exists, the
        // Download button sits centred beneath it instead of beside it.
        inner.removeFromTop (rowHeight); // status text
        downloadButton.setBounds (inner.removeFromTop (rowHeight).withSizeKeepingCentre (140, rowHeight - 16));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! cardBounds.contains (e.getPosition()) && onClose)
            onClose();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey && onClose)
        {
            onClose();
            return true;
        }

        return false;
    }

private:
    static constexpr int padding = 28;
    static constexpr int rowHeight = 46;

    static void drawRule (juce::Graphics& g, juce::Rectangle<int>& area)
    {
        g.setColour (Theme::hairline);
        g.fillRect (area.getX(), area.getY(), area.getWidth(), 1);
    }

    static void drawRowLabel (juce::Graphics& g, juce::Rectangle<int> row, const juce::String& text)
    {
        g.setColour (Theme::textSecondary);
        g.setFont (Theme::labelFont (12.0f));
        g.drawText (text, row, juce::Justification::centredLeft);
    }

    juce::String statusText;
    juce::Rectangle<int> cardBounds;
    CloseXButton closeButton;
    juce::TextButton checkButton, downloadButton, autoCheckButton;
};

} // namespace onyverb::ui
