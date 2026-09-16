#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/OnyvaLookAndFeel.h"
#include "UI/HeaderBar.h"
#include "UI/PresetBar.h"
#include "UI/ModePillBar.h"
#include "UI/DecayCurveDisplay.h"
#include "UI/OrbVisualizer.h"
#include "UI/CorrelationMeter.h"
#include "UI/KnobWithLabel.h"
#include "UI/ThemeSwitcher.h"
#include "UI/ParticleOverlay.h"
#include "UI/EcoModeButton.h"
#include "UI/InsaneModeButton.h"

namespace onyverb
{

/** The full custom ONY Verb editor: header/branding, preset browser, mode
    pills, the live decay-curve strip, the audio-reactive orb, gain rails,
    the Character/Diffusion slider, and the parameter knob grid. */
class OnyVerbEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit OnyVerbEditor (OnyVerbProcessor& p);
    ~OnyVerbEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void buildKnobRow (juce::OwnedArray<ui::KnobWithLabel>& row, std::initializer_list<std::pair<const char*, const char*>> params,
                        bool emphasized = false);
    void layoutKnobRowCentered (juce::OwnedArray<ui::KnobWithLabel>& row, juce::Rectangle<int> area, int maxSlotWidth = 0);
    void layoutKnobRowAligned (juce::OwnedArray<ui::KnobWithLabel>& row, juce::Rectangle<int> area, int columnWidth);
    void applyTheme (int index, bool save);
    void setAdvancedVisible (bool visible, bool save);
    void setInsaneMode (bool enabled, bool save);
    void setEcoMode (bool enabled, bool save);
    void refreshAllThemedComponents();

    /** Drives the Acid Trip theme's rainbow hue-cycle — only running while
        that theme is selected (started/stopped in applyTheme()). */
    void timerCallback() override;
    float acidHuePhase = 0.0f;

    OnyVerbProcessor& onyProcessor;
    ui::OnyvaLookAndFeel lookAndFeel;

    ui::HeaderBar header;
    ui::PresetBar presetBar;
    ui::ThemeSwitcher themeSwitcher;
    ui::ModePillBar modePills;
    ui::DecayCurveDisplay decayCurve;
    ui::OrbVisualizer orb;
    ui::CorrelationMeter correlationMeter;
    ui::CharacterSlider diffusionSlider;
    ui::CharacterSlider decaySlider;
    ui::VerticalFader inputFader, outputFader;
    ui::ParticleOverlay particleOverlay;

    juce::TextButton freezeButton;
    juce::ButtonParameterAttachment freezeAttachment;

    juce::TextButton advancedToggle;
    bool advancedExpanded = true;
    juce::Label madeWithLoveLabel;
    juce::Label developedByLabel;
    juce::Label versionLabel;

    ui::InsaneModeButton insaneModeButton;
    bool insaneMode = false;

    ui::EcoModeButton ecoModeButton;
    bool ecoMode = false;

    juce::OwnedArray<ui::KnobWithLabel> knobRowFeatured; // Mix, Low Cut, High Cut — bigger, above the rest
    juce::OwnedArray<ui::KnobWithLabel> knobRowA;        // Size, Pre-Delay, Width, Early Reflections
    juce::OwnedArray<ui::KnobWithLabel> knobRowB;        // Damping, Mod Depth, Mod Rate — the collapsible "Advanced" row

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OnyVerbEditor)
};

} // namespace onyverb
