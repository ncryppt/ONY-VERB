#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace onyverb::ui
{

/** Custom-drawn controls throughout ONY Verb: rotary knobs with a glow ring
    that tracks value, vertical gain rails, the horizontal Character/Diffusion
    slider, and pill-style tab buttons for the mode selector. Deliberately not
    the stock JUCE look — this is the whole "boutique, not a demo" ask. */
class OnyvaLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    OnyvaLookAndFeel() { refreshColours(); }

    /** Re-applies every base colour from the current Theme:: values. Called
        once at construction and again whenever the user switches themes,
        since setColour() bakes the value in at call time rather than
        reading Theme:: live. */
    void refreshColours()
    {
        setColour (juce::ResizableWindow::backgroundColourId, Theme::background);
        setColour (juce::Slider::textBoxTextColourId, Theme::textPrimary);
        setColour (juce::ComboBox::backgroundColourId, Theme::panelRaised);
        setColour (juce::ComboBox::outlineColourId, Theme::hairline);
        setColour (juce::ComboBox::textColourId, Theme::textPrimary);
        setColour (juce::PopupMenu::backgroundColourId, Theme::panelRaised);
        setColour (juce::PopupMenu::textColourId, Theme::textPrimary);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::accentDim);
        setColour (juce::TextButton::buttonColourId, Theme::panelRaised);
        setColour (juce::TextButton::textColourOffId, Theme::textSecondary);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                            float sliderPosProportional, float rotaryStartAngle,
                            float rotaryEndAngle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
        auto centre = bounds.getCentre();
        auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        auto trackRadius = radius * 0.82f;
        auto trackThickness = juce::jmax (2.5f, radius * 0.12f);

        // Background track.
        juce::Path bgArc;
        bgArc.addCentredArc (centre.x, centre.y, trackRadius, trackRadius, 0.0f,
                              rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (Theme::hairline);
        g.strokePath (bgArc, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Value arc, glowing.
        if (sliderPosProportional > 0.001f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc (centre.x, centre.y, trackRadius, trackRadius, 0.0f,
                                     rotaryStartAngle, angle, true);

            g.setColour (Theme::accent.withAlpha (0.25f));
            g.strokePath (valueArc, juce::PathStrokeType (trackThickness * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (Theme::accent);
            g.strokePath (valueArc, juce::PathStrokeType (trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Knob face with soft drop shadow.
        auto faceRadius = radius * 0.58f;
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (centre.x - faceRadius, centre.y - faceRadius + faceRadius * 0.12f, faceRadius * 2.0f, faceRadius * 2.0f);

        juce::ColourGradient faceGrad (Theme::panelRaised.brighter (0.08f), centre.x, centre.y - faceRadius,
                                        Theme::panel.darker (0.2f), centre.x, centre.y + faceRadius, false);
        g.setGradientFill (faceGrad);
        g.fillEllipse (centre.x - faceRadius, centre.y - faceRadius, faceRadius * 2.0f, faceRadius * 2.0f);
        g.setColour (Theme::hairline);
        g.drawEllipse (centre.x - faceRadius, centre.y - faceRadius, faceRadius * 2.0f, faceRadius * 2.0f, 1.0f);

        // Pointer.
        juce::Path pointer;
        auto pointerLen = faceRadius * 0.78f;
        pointer.addRoundedRectangle (-1.6f, -pointerLen, 3.2f, pointerLen * 0.62f, 1.6f);
        g.setColour (Theme::accent);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));

        juce::ignoreUnused (slider);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                            float sliderPos, float minSliderPos, float maxSliderPos,
                            const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);

        if (slider.isHorizontal())
        {
            drawCharacterSliderTrack (g, bounds, sliderPos, minSliderPos, maxSliderPos);
            return;
        }

        drawGainRail (g, bounds, sliderPos, minSliderPos, maxSliderPos);
        juce::ignoreUnused (style);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        auto isPill = button.getProperties().contains ("pill");
        auto radius = isPill ? bounds.getHeight() * 0.5f : Theme::cornerRadius;

        if (button.getToggleState())
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
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        g.setColour (button.getToggleState() ? Theme::accent : Theme::textSecondary);
        g.setFont (Theme::labelFont (13.5f));
        g.drawFittedText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, 1);
    }

    juce::Font getLabelFont (juce::Label&) override { return Theme::labelFont (13.0f); }

private:
    static void drawGainRail (juce::Graphics& g, juce::Rectangle<float> bounds,
                               float sliderPos, float minPos, float maxPos)
    {
        auto trackWidth = 5.0f;
        auto track = bounds.withWidth (trackWidth).withCentre ({ bounds.getCentreX(), bounds.getCentreY() });

        g.setColour (Theme::hairline);
        g.fillRoundedRectangle (track, trackWidth * 0.5f);

        auto centrePos = (minPos + maxPos) * 0.5f; // 0 dB sits mid-travel
        auto fillTop = juce::jmin (sliderPos, centrePos);
        auto fillBottom = juce::jmax (sliderPos, centrePos);
        auto fillRect = juce::Rectangle<float> (track.getX(), fillTop, trackWidth, fillBottom - fillTop);

        g.setColour (Theme::accent.withAlpha (0.7f));
        g.fillRoundedRectangle (fillRect, trackWidth * 0.5f);

        auto handleW = bounds.getWidth() * 0.85f;
        auto handleH = 14.0f;
        auto handle = juce::Rectangle<float> (0, 0, handleW, handleH).withCentre ({ bounds.getCentreX(), sliderPos });

        g.setColour (Theme::accent.withAlpha (0.3f));
        g.fillRoundedRectangle (handle.expanded (3.0f), handleH * 0.5f);
        g.setColour (Theme::panelRaised.brighter (0.1f));
        g.fillRoundedRectangle (handle, handleH * 0.5f);
        g.setColour (Theme::accent);
        g.drawRoundedRectangle (handle, handleH * 0.5f, 1.4f);
    }

    static void drawCharacterSliderTrack (juce::Graphics& g, juce::Rectangle<float> bounds,
                                           float sliderPos, float minPos, float maxPos)
    {
        auto trackHeight = 6.0f;
        auto track = bounds.withHeight (trackHeight).withCentre ({ bounds.getCentreX(), bounds.getCentreY() });

        juce::ColourGradient grad (Theme::accentDim, track.getX(), 0,
                                    Theme::accent, track.getRight(), 0, false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (track, trackHeight * 0.5f);
        g.setColour (Theme::background.withAlpha (0.35f));
        g.fillRoundedRectangle (juce::Rectangle<float> (sliderPos, track.getY(), juce::jmax (0.0f, maxPos - sliderPos), trackHeight), trackHeight * 0.5f);

        auto handleR = 9.0f;
        auto handleCentre = juce::Point<float> (sliderPos, bounds.getCentreY());

        g.setColour (Theme::accent.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (handleR * 2.6f, handleR * 2.6f).withCentre (handleCentre));
        g.setColour (Theme::panelRaised.brighter (0.15f));
        g.fillEllipse (juce::Rectangle<float> (handleR * 2.0f, handleR * 2.0f).withCentre (handleCentre));
        g.setColour (Theme::accent);
        g.drawEllipse (juce::Rectangle<float> (handleR * 2.0f, handleR * 2.0f).withCentre (handleCentre), 1.6f);

        juce::ignoreUnused (minPos);
    }
};

} // namespace onyverb::ui
