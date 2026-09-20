#include "PluginEditor.h"
#include <cmath>

namespace onyverb
{

namespace
{
constexpr int headerHeight = 62;
constexpr int presetBarHeight = 30;
constexpr int modePillHeight = 38;
constexpr int decayCurveHeight = 160;
constexpr int railWidth = 56;
constexpr int diffusionHeight = 46;
constexpr int sliderGap = 12;
constexpr int featuredRowHeight = 148;
constexpr int knobRowHeight = 116;
constexpr int knobRowBHeight = 94;   // the Advanced row is deliberately a step smaller than the row above it
constexpr int correlationWidth = 90;
constexpr int correlationHeight = 34;
constexpr int advancedToggleHeight = 24;
constexpr int footerHeight = 56;
constexpr int knobsPanelGap = 10;     // breathing room between the sliders panel and the knob-rows panel
constexpr int knobsPanelPadding = 8;  // breathing room between each panel's edge and its own contents

// Matches the current GitHub release tag — bump this by hand alongside each
// release until this is wired up to the actual build/CI version.
constexpr const char* versionString = "v0.1.8";

juce::File getAdvancedStateFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("ONYVA").getChildFile ("ONY Verb").getChildFile ("advanced.txt");
}

bool loadSavedAdvancedState()
{
    auto file = getAdvancedStateFile();
    if (! file.existsAsFile())
        return true; // expanded by default, so nothing changes for anyone until they collapse it once

    return file.loadFileAsString().trim() != "0";
}

void saveAdvancedState (bool expanded)
{
    auto file = getAdvancedStateFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText (expanded ? "1" : "0");
}

juce::File getInsaneStateFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("ONYVA").getChildFile ("ONY Verb").getChildFile ("insane.txt");
}

bool loadSavedInsaneState()
{
    auto file = getInsaneStateFile();
    return file.existsAsFile() && file.loadFileAsString().trim() == "1";
}

void saveInsaneState (bool enabled)
{
    auto file = getInsaneStateFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText (enabled ? "1" : "0");
}

juce::File getEcoStateFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("ONYVA").getChildFile ("ONY Verb").getChildFile ("eco.txt");
}

bool loadSavedEcoState()
{
    auto file = getEcoStateFile();
    return file.existsAsFile() && file.loadFileAsString().trim() == "1";
}

void saveEcoState (bool enabled)
{
    auto file = getEcoStateFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText (enabled ? "1" : "0");
}
}

OnyVerbContent::OnyVerbContent (OnyVerbProcessor& p)
    : onyProcessor (p),
      header (p.apvts),
      presetBar (p.apvts),
      modePills (p.apvts),
      decayCurve (p.apvts, p.getVisualizationRingBuffer(), p.getSpectrumRingBuffer()),
      orb (p.getVisualizationRingBuffer(), p.apvts.getRawParameterValue (ParamIDs::freeze)),
      correlationMeter (p.getVisualizationRingBuffer()),
      diffusionSlider (p.apvts, ParamIDs::diffusion, "Character"),
      decaySlider (p.apvts, ParamIDs::decayTime, "Decay"),
      inputFader (p.apvts, ParamIDs::inputGain, "In"),
      outputFader (p.apvts, ParamIDs::outputGain, "Out")
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (header);
    addAndMakeVisible (presetBar);
    addAndMakeVisible (themeSwitcher);
    addAndMakeVisible (modePills);
    addAndMakeVisible (decayCurve);
    addAndMakeVisible (orb);
    addAndMakeVisible (correlationMeter);
    addAndMakeVisible (diffusionSlider);
    addAndMakeVisible (decaySlider);
    addAndMakeVisible (inputFader);
    addAndMakeVisible (outputFader);

    buildKnobRow (knobRowFeatured, { { ParamIDs::lowCut, "Low Cut" }, { ParamIDs::dryLevel, "Dry" },
                                      { ParamIDs::wetLevel, "Wet" }, { ParamIDs::highCut, "High Cut" } }, true);

    buildKnobRow (knobRowA, { { ParamIDs::size, "Size" }, { ParamIDs::preDelay, "Pre-Delay" },
                              { ParamIDs::width, "Width" }, { ParamIDs::earlyLevel, "Early Refl." } });

    buildKnobRow (knobRowB, { { ParamIDs::damping, "Damping" },
                              { ParamIDs::modDepth, "Mod Depth" }, { ParamIDs::modRate, "Mod Rate" } });

    advancedToggle.setClickingTogglesState (false);
    advancedToggle.getProperties().set ("pill", true);
    addAndMakeVisible (advancedToggle);
    advancedToggle.onClick = [this] { setAdvancedVisible (! advancedExpanded, true); };

    // Written pre-uppercased (rather than via .toUpperCase()) since that
    // was leaving the accented E in "QUÉBEC" as a lowercase é.
    madeWithLoveLabel.setText (juce::String::fromUTF8 ("MADE WITH \xe2\x99\xa5 IN QU" "\xc3\x89" "BEC CITY"), juce::dontSendNotification);
    madeWithLoveLabel.setJustificationType (juce::Justification::centred);
    madeWithLoveLabel.setFont (ui::Theme::labelFont (11.0f));
    madeWithLoveLabel.setColour (juce::Label::textColourId, ui::Theme::textDim);
    madeWithLoveLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (madeWithLoveLabel);

    developedByLabel.setText ("DEVELOPED BY ON Y VA RECORDS", juce::dontSendNotification);
    developedByLabel.setJustificationType (juce::Justification::centred);
    developedByLabel.setFont (ui::Theme::labelFont (11.0f));
    developedByLabel.setColour (juce::Label::textColourId, ui::Theme::textDim);
    developedByLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (developedByLabel);

    versionLabel.setText (versionString, juce::dontSendNotification);
    versionLabel.setJustificationType (juce::Justification::centred);
    versionLabel.setFont (ui::Theme::labelFont (10.0f));
    versionLabel.setColour (juce::Label::textColourId, ui::Theme::textDim);
    versionLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (versionLabel);

    setAdvancedVisible (loadSavedAdvancedState(), false);

    themeSwitcher.onThemeChanged = [this] (int index) { applyTheme (index, true); };
    auto savedThemeIndex = ui::Theme::loadSavedThemeIndex();
    themeSwitcher.setSelectedIndex (savedThemeIndex);
    applyTheme (savedThemeIndex, false);

    // Added last so it paints on top of every other control; click-through
    // (set in its own constructor) means it never steals mouse input.
    addAndMakeVisible (particleOverlay);
    particleOverlay.setLivelinessSource ([this] { return orb.getLiveliness(); });
    orb.onTransient = [this] { particleOverlay.spawnBurst(); };

    addAndMakeVisible (insaneModeButton);
    insaneModeButton.onClick = [this] { setInsaneMode (! insaneMode, true); };
    setInsaneMode (loadSavedInsaneState(), false);

    addAndMakeVisible (ecoModeButton);
    ecoModeButton.onClick = [this] { setEcoMode (! ecoMode, true); };
    setEcoMode (loadSavedEcoState(), false);

    // Every button gets the same click-pop the orb gives its own transients
    // — wired last, once every button's real onClick is already in place,
    // since this wraps (rather than replaces) whatever was there.
    ui::wireClickBurst (advancedToggle, particleOverlay);
    ui::wireClickBurst (ecoModeButton, particleOverlay);
    ui::wireClickBurst (insaneModeButton, particleOverlay);
    header.forEachButton ([this] (juce::Button& b) { ui::wireClickBurst (b, particleOverlay); });
    modePills.forEachButton ([this] (juce::Button& b) { ui::wireClickBurst (b, particleOverlay); });
    presetBar.forEachButton ([this] (juce::Button& b) { ui::wireClickBurst (b, particleOverlay); });

    // Same idea while a knob/slider is actively being dragged, rather than
    // just on click — a light continuous trickle instead of one pop.
    diffusionSlider.wireParticles (particleOverlay);
    decaySlider.wireParticles (particleOverlay);
    inputFader.wireParticles (particleOverlay);
    outputFader.wireParticles (particleOverlay);
    for (auto* row : { &knobRowFeatured, &knobRowA, &knobRowB })
        for (auto* knob : *row)
            knob->wireParticles (particleOverlay);

    setSize (OnyVerbEditor::designWidth, OnyVerbEditor::designHeight);
}

OnyVerbContent::~OnyVerbContent()
{
    setLookAndFeel (nullptr);
}

void OnyVerbContent::buildKnobRow (juce::OwnedArray<ui::KnobWithLabel>& row,
                                   std::initializer_list<std::pair<const char*, const char*>> params,
                                   bool emphasized)
{
    for (auto& [paramID, name] : params)
    {
        auto* knob = row.add (new ui::KnobWithLabel (onyProcessor.apvts, paramID, name, emphasized));
        addAndMakeVisible (knob);
    }
}

void OnyVerbContent::paint (juce::Graphics& g)
{
    g.fillAll (ui::Theme::background);

    if (ui::Theme::acidTripActive)
    {
        // A slow, low-alpha two-colour wash across the whole window on top
        // of the flat background — subtle enough to keep everything legible,
        // but it keeps the backdrop itself breathing colour along with the
        // accent-driven controls.
        auto bounds = getLocalBounds().toFloat();
        auto washA = juce::Colour::fromHSV (acidHuePhase, 0.85f, 1.0f, 0.07f);
        auto washB = juce::Colour::fromHSV (std::fmod (acidHuePhase + 0.5f, 1.0f), 0.85f, 1.0f, 0.06f);
        juce::ColourGradient wash (washA, bounds.getX(), bounds.getY(), washB, bounds.getRight(), bounds.getBottom(), false);
        g.setGradientFill (wash);
        g.fillRect (bounds);
    }

    if (! slidersPanelBounds.isEmpty())
        ui::Theme::fillIndentedRoundedRect (g, slidersPanelBounds.toFloat().reduced (2.0f), 14.0f);

    if (! featuredPanelBounds.isEmpty())
        ui::Theme::fillIndentedRoundedRect (g, featuredPanelBounds.toFloat().reduced (2.0f), 14.0f);

    if (! knobsPanelBounds.isEmpty())
        ui::Theme::fillIndentedRoundedRect (g, knobsPanelBounds.toFloat().reduced (2.0f), 14.0f);
}

void OnyVerbContent::resized()
{
    particleOverlay.setBounds (getLocalBounds());

    auto b = getLocalBounds();
    b.removeFromTop (8); // keep the header row (and Bypass button) off the window edge

    auto headerRow = b.removeFromTop (headerHeight);
    header.setBounds (headerRow);

    auto headerButtons = headerRow.withSizeKeepingCentre (90 + 8 + 84, 26);
    ecoModeButton.setBounds (headerButtons.removeFromLeft (84));
    headerButtons.removeFromLeft (8);
    insaneModeButton.setBounds (headerButtons);

    auto presetRow = b.removeFromTop (presetBarHeight);
    presetRow.removeFromLeft (10); // keep the theme switcher off the window edge
    themeSwitcher.setBounds (presetRow.removeFromLeft (150).reduced (4, 2));
    presetRow.removeFromRight (150); // mirror the theme switcher's width so the preset combo stays centred
    presetBar.setBounds (presetRow.withSizeKeepingCentre (juce::jmin (420, presetRow.getWidth() - 20), presetBarHeight));

    b.removeFromTop (6); // breathing room between the theme/preset row and the mode pills

    modePills.setBounds (b.removeFromTop (modePillHeight).reduced (12, 3));
    decayCurve.setBounds (b.removeFromTop (decayCurveHeight).reduced (10, 6));

    b.removeFromBottom (8); // breathing room below the last footer line, off the window edge
    auto footerArea = b.removeFromBottom (footerHeight);
    constexpr int footerLineGap = 4;
    auto footerLineHeight = (footerHeight - footerLineGap * 2) / 3;
    madeWithLoveLabel.setBounds (footerArea.removeFromTop (footerLineHeight));
    footerArea.removeFromTop (footerLineGap);
    developedByLabel.setBounds (footerArea.removeFromTop (footerLineHeight));
    footerArea.removeFromTop (footerLineGap);
    versionLabel.setBounds (footerArea);

    b.removeFromBottom (10); // breathing room between the last knob row and the footer text

    auto rowsHeight = featuredRowHeight + knobRowHeight + advancedToggleHeight
                     + (advancedExpanded ? knobRowBHeight : 0) + diffusionHeight * 2 + sliderGap
                     + knobsPanelGap * 2 + knobsPanelPadding * 6;
    auto mainArea = b.reduced (8, 4);

    auto leftRail = mainArea.removeFromLeft (railWidth);
    auto rightRail = mainArea.removeFromRight (railWidth);

    correlationMeter.setBounds (rightRail.removeFromBottom (correlationHeight).withWidth (correlationWidth).withX (rightRail.getX() - (correlationWidth - railWidth) / 2));
    inputFader.setBounds (leftRail.reduced (6));
    outputFader.setBounds (rightRail.reduced (6));

    // The knob rows below are fixed-pixel heights that don't scale with the
    // window, so on a small enough window (or with Advanced toggled open at
    // a size that was fine collapsed) rowsHeight can exceed what's left of
    // mainArea — clamping it here guarantees the orb keeps at least
    // minOrbHeight rather than being squeezed down to nothing.
    constexpr int minOrbHeight = 90;
    auto centreArea = mainArea;
    auto knobsHeight = juce::jmin (rowsHeight, juce::jmax (0, centreArea.getHeight() - minOrbHeight));
    auto knobsArea = centreArea.removeFromBottom (knobsHeight);

    auto orbArea = centreArea.reduced (0, 14); // keeps the orb's glow/pulses from reaching under the graph and knob panels
    auto orbSize = juce::jmax (40, juce::jmin (orbArea.getWidth(), orbArea.getHeight()));
    orb.setBounds (orbArea.withSizeKeepingCentre (orbSize, orbSize));
    particleOverlay.setOrbGeometry (orb.getBounds().toFloat().getCentre(), (float) orbSize * 0.5f * 0.6f);

    // Character/Decay, the featured knob row, and the remaining knob rows
    // each sit in their own separate indented panel (see paint()) rather
    // than sharing one card, each with a little internal padding so the
    // sliders/knobs don't crowd right up against the panel edge.
    auto slidersArea = knobsArea.removeFromTop (diffusionHeight * 2 + sliderGap + knobsPanelPadding * 2);
    slidersPanelBounds = slidersArea;
    auto slidersContent = slidersArea.reduced (0, knobsPanelPadding);
    diffusionSlider.setBounds (slidersContent.removeFromTop (diffusionHeight).reduced (20, 2));
    slidersContent.removeFromTop (sliderGap);
    decaySlider.setBounds (slidersContent.removeFromTop (diffusionHeight).reduced (20, 2));

    knobsArea.removeFromTop (knobsPanelGap);
    auto featuredArea = knobsArea.removeFromTop (featuredRowHeight + knobsPanelPadding * 2);
    featuredPanelBounds = featuredArea;
    layoutKnobRowCentered (knobRowFeatured, featuredArea.reduced (0, knobsPanelPadding), 170);

    knobsArea.removeFromTop (knobsPanelGap);
    knobsPanelBounds = knobsArea;
    auto knobsContent = knobsArea.reduced (0, knobsPanelPadding);

    // Rows A and B share one column grid (sized off the wider row) so knobs
    // that stack vertically actually line up, instead of each row stretching
    // its own knob count independently across the full width.
    auto columnWidth = knobsContent.getWidth() / juce::jmax (knobRowA.size(), knobRowB.size());
    layoutKnobRowAligned (knobRowA, knobsContent.removeFromTop (knobRowHeight), columnWidth);

    auto toggleRow = knobsContent.removeFromTop (advancedToggleHeight);
    advancedToggle.setBounds (toggleRow.withSizeKeepingCentre (120, advancedToggleHeight - 4));

    if (advancedExpanded)
        layoutKnobRowAligned (knobRowB, knobsContent.removeFromTop (knobRowBHeight), columnWidth);
}

void OnyVerbContent::layoutKnobRowCentered (juce::OwnedArray<ui::KnobWithLabel>& row, juce::Rectangle<int> area, int maxSlotWidth)
{
    if (row.isEmpty()) return;

    auto naturalWidth = area.getWidth() / row.size();
    auto w = maxSlotWidth > 0 ? juce::jmin (naturalWidth, maxSlotWidth) : naturalWidth;
    auto totalWidth = w * row.size();
    auto startX = area.getX() + (area.getWidth() - totalWidth) / 2;

    for (int i = 0; i < row.size(); ++i)
        row[i]->setBounds (startX + i * w, area.getY(), w, area.getHeight());
}

void OnyVerbContent::layoutKnobRowAligned (juce::OwnedArray<ui::KnobWithLabel>& row, juce::Rectangle<int> area, int columnWidth)
{
    if (row.isEmpty()) return;

    // Centered as a group within the shared grid so a shorter row (e.g. 3
    // knobs against a 4-column grid) sits in the middle rather than jammed
    // to one side, while every row still uses the exact same column width.
    auto totalWidth = columnWidth * row.size();
    auto startX = area.getX() + (area.getWidth() - totalWidth) / 2;

    for (int i = 0; i < row.size(); ++i)
        row[i]->setBounds (startX + i * columnWidth, area.getY(), columnWidth, area.getHeight());
}

void OnyVerbContent::applyTheme (int index, bool save)
{
    auto& palettes = ui::Theme::getThemePalettes();
    if (! juce::isPositiveAndBelow (index, (int) palettes.size()))
        return;

    ui::Theme::applyPalette (palettes[(size_t) index]);
    refreshAllThemedComponents();

    // Acid Trip owns accent/accentDim/accentGlow continuously from here on
    // (see timerCallback()) rather than being one static palette; every
    // other theme just needs the one-off refresh above.
    if (ui::Theme::acidTripActive)
        startTimerHz (ecoMode ? 15 : 30);
    else
        stopTimer();

    if (save)
        ui::Theme::saveThemeIndex (index);
}

void OnyVerbContent::refreshAllThemedComponents()
{
    lookAndFeel.refreshColours();

    // Everything else reads Theme:: fresh on every paint() and just needs a
    // repaint; these few components called Component::setColour(...) once
    // at construction time and need that explicitly re-applied.
    header.refreshTheme();
    diffusionSlider.refreshTheme();
    decaySlider.refreshTheme();
    inputFader.refreshTheme();
    outputFader.refreshTheme();
    for (auto* knob : knobRowFeatured) knob->refreshTheme();
    for (auto* knob : knobRowA) knob->refreshTheme();
    for (auto* knob : knobRowB) knob->refreshTheme();
    madeWithLoveLabel.setColour (juce::Label::textColourId, ui::Theme::textDim);
    developedByLabel.setColour (juce::Label::textColourId, ui::Theme::textDim);
    versionLabel.setColour (juce::Label::textColourId, ui::Theme::textDim);

    sendLookAndFeelChange();
    repaint();
}

void OnyVerbContent::timerCallback()
{
    acidHuePhase += 0.006f;
    if (acidHuePhase > 1.0f)
        acidHuePhase -= 1.0f;

    ui::Theme::accent = juce::Colour::fromHSV (acidHuePhase, 0.9f, 1.0f, 1.0f);
    ui::Theme::accentDim = ui::Theme::accent.darker (0.55f);
    ui::Theme::accentGlow = ui::Theme::accent.withAlpha (0.5f);

    refreshAllThemedComponents();
}

void OnyVerbContent::setAdvancedVisible (bool visible, bool save)
{
    advancedExpanded = visible;

    for (auto* knob : knobRowB)
        knob->setVisible (visible);

    inputFader.setVisible (visible);
    outputFader.setVisible (visible);
    correlationMeter.setVisible (visible);

    advancedToggle.setButtonText ("Advanced");
    advancedToggle.setToggleState (visible, juce::dontSendNotification);
    madeWithLoveLabel.setVisible (visible);
    developedByLabel.setVisible (visible);
    versionLabel.setVisible (visible);

    resized();
    repaint();

    if (save)
        saveAdvancedState (visible);
}

void OnyVerbContent::setInsaneMode (bool enabled, bool save)
{
    insaneMode = enabled;
    insaneModeButton.setToggleState (enabled, juce::dontSendNotification);
    particleOverlay.setInsaneMode (enabled);

    if (save)
        saveInsaneState (enabled);
}

void OnyVerbContent::setEcoMode (bool enabled, bool save)
{
    ecoMode = enabled;
    ecoModeButton.setToggleState (enabled, juce::dontSendNotification);

    orb.setEcoMode (enabled);
    particleOverlay.setEcoMode (enabled);
    decayCurve.setEcoMode (enabled);
    correlationMeter.setEcoMode (enabled);

    // Restart the Acid Trip hue-cycle at the right rate if it's the active
    // theme, rather than waiting for the next theme switch to pick it up.
    if (ui::Theme::acidTripActive)
        startTimerHz (enabled ? 15 : 30);

    if (save)
        saveEcoState (enabled);
}

OnyVerbEditor::OnyVerbEditor (OnyVerbProcessor& p)
    : juce::AudioProcessorEditor (p),
      content (p)
{
    addAndMakeVisible (content);

    // Opens at ~70% of the design size so it fits a 13-inch laptop screen
    // (with room for the host's own title bar) instead of ~970px tall.
    constexpr double defaultScale = 0.59;
    constexpr double minScale = 0.5, maxScale = 1.5;
    sizeConstrainer.setFixedAspectRatio ((double) designWidth / (double) designHeight);
    sizeConstrainer.setSizeLimits ((int) (designWidth * minScale), (int) (designHeight * minScale),
                                   (int) (designWidth * maxScale), (int) (designHeight * maxScale));
    setConstrainer (&sizeConstrainer);
    setResizable (true, true);
    setSize ((int) (designWidth * defaultScale), (int) (designHeight * defaultScale));
}

void OnyVerbEditor::resized()
{
    content.setBounds (0, 0, designWidth, designHeight);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) designWidth,
                                                       (float) getHeight() / (float) designHeight));
}

} // namespace onyverb
