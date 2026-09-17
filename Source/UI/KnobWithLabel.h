#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "ParticleOverlay.h"

namespace onyverb::ui
{

/** A rotary knob (drawn by OnyvaLookAndFeel) plus a name caption and a live
    value readout — the repeated unit for Size/Decay/Pre-Delay/Damping/Mod
    Depth/Mod Rate/Early Reflections/Mix. */
class KnobWithLabel final : public juce::Component
{
public:
    KnobWithLabel (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& displayName,
                    bool emphasizedIn = false)
        : slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox),
          attachment (*apvts.getParameter (paramID), slider, apvts.undoManager),
          emphasized (emphasizedIn)
    {
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible (slider);

        nameLabel.setText (displayName, juce::dontSendNotification);
        nameLabel.setJustificationType (juce::Justification::centred);
        nameLabel.setColour (juce::Label::textColourId, Theme::textSecondary);
        nameLabel.setFont (Theme::labelFont (emphasized ? 13.5f : 11.0f));
        addAndMakeVisible (nameLabel);

        valueLabel.setJustificationType (juce::Justification::centred);
        valueLabel.setColour (juce::Label::textColourId, Theme::accent);
        valueLabel.setFont (Theme::labelFont (emphasized ? 12.5f : 10.5f));
        valueLabel.getProperties().set ("lcd", true);
        addAndMakeVisible (valueLabel);

        slider.onValueChange = [this] { updateValueLabel(); };
        updateValueLabel();
    }

    void resized() override
    {
        auto b = getLocalBounds();
        auto nameHeight = emphasized ? 19 : 15;
        auto valueHeight = emphasized ? 20 : 16;
        nameLabel.setBounds (b.removeFromTop (nameHeight));

        auto valueBounds = b.removeFromBottom (valueHeight);
        auto chipWidth = juce::jmin (valueBounds.getWidth() - 8, emphasized ? 56 : 46);
        valueLabel.setBounds (valueBounds.withSizeKeepingCentre (chipWidth, valueBounds.getHeight()));

        slider.setBounds (b.reduced (emphasized ? 4 : 2));
    }

    /** Labels bake their colour in at setColour() time rather than reading
        Theme:: live, so a theme switch needs this re-applied explicitly. */
    void refreshTheme()
    {
        nameLabel.setColour (juce::Label::textColourId, Theme::textSecondary);
        valueLabel.setColour (juce::Label::textColourId, Theme::accent);
        repaint();
    }

    void wireParticles (ParticleOverlay& overlay) { wireDragTrickle (slider, overlay); }

private:
    void updateValueLabel()
    {
        valueLabel.setText (slider.getTextFromValue (slider.getValue()), juce::dontSendNotification);
        valueLabel.getProperties().set ("lcdOn", slider.getValue() > 0.0);
        valueLabel.repaint();
    }

    juce::Slider slider;
    juce::SliderParameterAttachment attachment;
    juce::Label nameLabel, valueLabel;
    bool emphasized = false;
};

/** The horizontal "Character" (Diffusion) slider — gradient track, single
    glowing handle, drawn by OnyvaLookAndFeel's horizontal branch. Same
    attachment pattern as KnobWithLabel, just a different Slider style. */
class CharacterSlider final : public juce::Component
{
public:
    CharacterSlider (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& displayName)
        : slider (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox),
          attachment (*apvts.getParameter (paramID), slider, apvts.undoManager),
          unitLabel (apvts.getParameter (paramID)->getLabel())
    {
        addAndMakeVisible (slider);

        nameLabel.setText (displayName, juce::dontSendNotification);
        nameLabel.setJustificationType (juce::Justification::centredLeft);
        nameLabel.setColour (juce::Label::textColourId, Theme::textSecondary);
        nameLabel.setFont (Theme::labelFont (11.5f));
        addAndMakeVisible (nameLabel);

        valueLabel.setJustificationType (juce::Justification::centred);
        valueLabel.setColour (juce::Label::textColourId, Theme::accent);
        valueLabel.setFont (Theme::labelFont (11.5f));
        valueLabel.getProperties().set ("lcd", true);
        addAndMakeVisible (valueLabel);

        slider.onValueChange = [this] { updateValueLabel(); };
        updateValueLabel();
    }

    void resized() override
    {
        auto b = getLocalBounds();
        auto topRow = b.removeFromTop (18);
        valueLabel.setBounds (topRow.removeFromRight (62).reduced (0, 1));
        topRow.removeFromRight (6);
        nameLabel.setBounds (topRow);
        slider.setBounds (b.reduced (4, 0));
    }

    void refreshTheme()
    {
        nameLabel.setColour (juce::Label::textColourId, Theme::textSecondary);
        valueLabel.setColour (juce::Label::textColourId, Theme::accent);
        repaint();
    }

    void wireParticles (ParticleOverlay& overlay) { wireDragTrickle (slider, overlay); }

private:
    void updateValueLabel()
    {
        auto text = slider.getTextFromValue (slider.getValue());
        if (unitLabel.isNotEmpty())
            text << " " << unitLabel;
        valueLabel.setText (text, juce::dontSendNotification);
        valueLabel.getProperties().set ("lcdOn", slider.getValue() > 0.0);
        valueLabel.repaint();
    }

    juce::Slider slider;
    juce::SliderParameterAttachment attachment;
    juce::String unitLabel;
    juce::Label nameLabel, valueLabel;
};

/** Vertical gain-trim rail for the left/right edges (Input/Output gain),
    mirroring the reference image's fader rails. */
class VerticalFader final : public juce::Component
{
public:
    VerticalFader (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& displayName)
        : slider (juce::Slider::LinearVertical, juce::Slider::NoTextBox),
          attachment (*apvts.getParameter (paramID), slider, apvts.undoManager)
    {
        addAndMakeVisible (slider);

        nameLabel.setText (displayName, juce::dontSendNotification);
        nameLabel.setJustificationType (juce::Justification::centred);
        nameLabel.setColour (juce::Label::textColourId, Theme::textSecondary);
        nameLabel.setFont (Theme::labelFont (10.5f));
        addAndMakeVisible (nameLabel);

        valueLabel.setJustificationType (juce::Justification::centred);
        valueLabel.setColour (juce::Label::textColourId, Theme::accent);
        valueLabel.setFont (Theme::labelFont (10.0f));
        valueLabel.getProperties().set ("lcd", true);
        addAndMakeVisible (valueLabel);

        slider.onValueChange = [this] { updateValueLabel(); };
        updateValueLabel();
    }

    void resized() override
    {
        auto b = getLocalBounds();
        nameLabel.setBounds (b.removeFromTop (15));

        auto valueBounds = b.removeFromBottom (16);
        auto chipWidth = juce::jmin (valueBounds.getWidth() - 4, 42);
        valueLabel.setBounds (valueBounds.withSizeKeepingCentre (chipWidth, valueBounds.getHeight()));

        slider.setBounds (b);
    }

    void refreshTheme()
    {
        nameLabel.setColour (juce::Label::textColourId, Theme::textSecondary);
        valueLabel.setColour (juce::Label::textColourId, Theme::accent);
        repaint();
    }

    void wireParticles (ParticleOverlay& overlay) { wireDragTrickle (slider, overlay); }

private:
    void updateValueLabel()
    {
        valueLabel.setText (slider.getTextFromValue (slider.getValue()), juce::dontSendNotification);
        valueLabel.getProperties().set ("lcdOn", slider.getValue() > 0.0);
        valueLabel.repaint();
    }

    juce::Slider slider;
    juce::SliderParameterAttachment attachment;
    juce::Label nameLabel, valueLabel;
};

} // namespace onyverb::ui
