#include "PluginEditor.h"
#include <cmath>

namespace onyverb
{

namespace
{
constexpr int headerHeight = 62;
constexpr int presetBarHeight = 30;
constexpr int modePillHeight = 38;
constexpr int decayCurveHeight = 108;
constexpr int railWidth = 56;
constexpr int diffusionHeight = 46;
constexpr int sliderGap = 12;
constexpr int featuredRowHeight = 148;
constexpr int knobRowHeight = 92;
constexpr int correlationWidth = 90;
constexpr int correlationHeight = 34;
constexpr int advancedToggleHeight = 24;

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
}

OnyVerbEditor::OnyVerbEditor (OnyVerbProcessor& p)
    : juce::AudioProcessorEditor (p),
      onyProcessor (p),
      header (p.apvts),
      presetBar (p.apvts),
      modePills (p.apvts),
      decayCurve (p.apvts),
      orb (p.getVisualizationRingBuffer(), p.apvts.getRawParameterValue (ParamIDs::freeze)),
      correlationMeter (p.getVisualizationRingBuffer()),
      diffusionSlider (p.apvts, ParamIDs::diffusion, "Character"),
      decaySlider (p.apvts, ParamIDs::decayTime, "Decay"),
      inputFader (p.apvts, ParamIDs::inputGain, "In"),
      outputFader (p.apvts, ParamIDs::outputGain, "Out"),
      freezeAttachment (*p.apvts.getParameter (ParamIDs::freeze), freezeButton, p.apvts.undoManager)
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

    freezeButton.setButtonText ("Freeze");
    freezeButton.getProperties().set ("pill", true);
    addAndMakeVisible (freezeButton);
    freezeAttachment.sendInitialUpdate();

    buildKnobRow (knobRowFeatured, { { ParamIDs::lowCut, "Low Cut" }, { ParamIDs::mix, "Mix" },
                                      { ParamIDs::highCut, "High Cut" } }, true);

    buildKnobRow (knobRowA, { { ParamIDs::size, "Size" }, { ParamIDs::preDelay, "Pre-Delay" },
                              { ParamIDs::width, "Width" }, { ParamIDs::earlyLevel, "Early Refl." } });

    buildKnobRow (knobRowB, { { ParamIDs::damping, "Damping" },
                              { ParamIDs::modDepth, "Mod Depth" }, { ParamIDs::modRate, "Mod Rate" } });

    advancedToggle.setClickingTogglesState (false);
    advancedToggle.getProperties().set ("pill", true);
    addAndMakeVisible (advancedToggle);
    advancedToggle.onClick = [this] { setAdvancedVisible (! advancedExpanded, true); };
    setAdvancedVisible (loadSavedAdvancedState(), false);

    header.onCompareRequested ([this] (char slot) { applyCompareSlot (slot); });
    compareSlotA = p.apvts.copyState();
    compareSlotB = compareSlotA.createCopy();

    themeSwitcher.onThemeChanged = [this] (int index) { applyTheme (index, true); };
    auto savedThemeIndex = ui::Theme::loadSavedThemeIndex();
    themeSwitcher.setSelectedIndex (savedThemeIndex);
    applyTheme (savedThemeIndex, false);

    // Added last so it paints on top of every other control; click-through
    // (set in its own constructor) means it never steals mouse input.
    addAndMakeVisible (particleOverlay);
    particleOverlay.setLivelinessSource ([this] { return orb.getLiveliness(); });
    orb.onTransient = [this] { particleOverlay.spawnBurst(); };

    insaneModeButton.setButtonText ("Insane");
    insaneModeButton.setClickingTogglesState (false);
    insaneModeButton.getProperties().set ("pill", true);
    addAndMakeVisible (insaneModeButton);
    insaneModeButton.onClick = [this] { setInsaneMode (! insaneMode, true); };
    setInsaneMode (loadSavedInsaneState(), false);

    setResizable (true, true);
    setResizeLimits (760, 560, 1600, 1200);
    setSize (960, 720);
}

OnyVerbEditor::~OnyVerbEditor()
{
    setLookAndFeel (nullptr);
}

void OnyVerbEditor::buildKnobRow (juce::OwnedArray<ui::KnobWithLabel>& row,
                                   std::initializer_list<std::pair<const char*, const char*>> params,
                                   bool emphasized)
{
    for (auto& [paramID, name] : params)
    {
        auto* knob = row.add (new ui::KnobWithLabel (onyProcessor.apvts, paramID, name, emphasized));
        addAndMakeVisible (knob);
    }
}

void OnyVerbEditor::paint (juce::Graphics& g)
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
}

void OnyVerbEditor::resized()
{
    particleOverlay.setBounds (getLocalBounds());

    auto b = getLocalBounds();

    auto headerRow = b.removeFromTop (headerHeight);
    header.setBounds (headerRow);
    insaneModeButton.setBounds (headerRow.withSizeKeepingCentre (90, 26));

    auto presetRow = b.removeFromTop (presetBarHeight);
    themeSwitcher.setBounds (presetRow.removeFromLeft (150).reduced (4, 2));
    presetRow.removeFromRight (150); // mirror the theme switcher's width so the preset combo stays centred
    presetBar.setBounds (presetRow.withSizeKeepingCentre (juce::jmin (420, presetRow.getWidth() - 20), presetBarHeight));

    modePills.setBounds (b.removeFromTop (modePillHeight).reduced (12, 3));
    decayCurve.setBounds (b.removeFromTop (decayCurveHeight).reduced (10, 6));

    auto rowsHeight = featuredRowHeight + knobRowHeight + advancedToggleHeight
                     + (advancedExpanded ? knobRowHeight : 0) + diffusionHeight * 2 + sliderGap;
    auto mainArea = b.reduced (8, 4);

    auto leftRail = mainArea.removeFromLeft (railWidth);
    auto rightRail = mainArea.removeFromRight (railWidth);

    correlationMeter.setBounds (rightRail.removeFromBottom (correlationHeight).withWidth (correlationWidth).withX (rightRail.getX() - (correlationWidth - railWidth) / 2));
    inputFader.setBounds (leftRail.reduced (6));
    outputFader.setBounds (rightRail.reduced (6));

    auto centreArea = mainArea;
    auto knobsArea = centreArea.removeFromBottom (rowsHeight);

    auto orbArea = centreArea;
    auto orbSize = juce::jmin (orbArea.getWidth(), orbArea.getHeight());
    orb.setBounds (orbArea.withSizeKeepingCentre (orbSize, orbSize));
    particleOverlay.setOrbGeometry (orb.getBounds().toFloat().getCentre(), (float) orbSize * 0.5f * 0.6f);

    freezeButton.setBounds (orbArea.getRight() - 84, orbArea.getY() + 6, 76, 26);

    diffusionSlider.setBounds (knobsArea.removeFromTop (diffusionHeight).reduced (20, 2));
    knobsArea.removeFromTop (sliderGap);
    decaySlider.setBounds (knobsArea.removeFromTop (diffusionHeight).reduced (20, 2));
    layoutKnobRowCentered (knobRowFeatured, knobsArea.removeFromTop (featuredRowHeight), 170);

    // Rows A and B share one column grid (sized off the wider row) so knobs
    // that stack vertically actually line up, instead of each row stretching
    // its own knob count independently across the full width.
    auto columnWidth = knobsArea.getWidth() / juce::jmax (knobRowA.size(), knobRowB.size());
    layoutKnobRowAligned (knobRowA, knobsArea.removeFromTop (knobRowHeight), columnWidth);

    auto toggleRow = knobsArea.removeFromTop (advancedToggleHeight);
    advancedToggle.setBounds (toggleRow.withSizeKeepingCentre (120, advancedToggleHeight - 4));

    if (advancedExpanded)
        layoutKnobRowAligned (knobRowB, knobsArea.removeFromTop (knobRowHeight), columnWidth);
}

void OnyVerbEditor::layoutKnobRowCentered (juce::OwnedArray<ui::KnobWithLabel>& row, juce::Rectangle<int> area, int maxSlotWidth)
{
    if (row.isEmpty()) return;

    auto naturalWidth = area.getWidth() / row.size();
    auto w = maxSlotWidth > 0 ? juce::jmin (naturalWidth, maxSlotWidth) : naturalWidth;
    auto totalWidth = w * row.size();
    auto startX = area.getX() + (area.getWidth() - totalWidth) / 2;

    for (int i = 0; i < row.size(); ++i)
        row[i]->setBounds (startX + i * w, area.getY(), w, area.getHeight());
}

void OnyVerbEditor::layoutKnobRowAligned (juce::OwnedArray<ui::KnobWithLabel>& row, juce::Rectangle<int> area, int columnWidth)
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

void OnyVerbEditor::applyTheme (int index, bool save)
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
        startTimerHz (30);
    else
        stopTimer();

    if (save)
        ui::Theme::saveThemeIndex (index);
}

void OnyVerbEditor::refreshAllThemedComponents()
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

    sendLookAndFeelChange();
    repaint();
}

void OnyVerbEditor::timerCallback()
{
    acidHuePhase += 0.006f;
    if (acidHuePhase > 1.0f)
        acidHuePhase -= 1.0f;

    ui::Theme::accent = juce::Colour::fromHSV (acidHuePhase, 0.9f, 1.0f, 1.0f);
    ui::Theme::accentDim = ui::Theme::accent.darker (0.55f);
    ui::Theme::accentGlow = ui::Theme::accent.withAlpha (0.5f);

    refreshAllThemedComponents();
}

void OnyVerbEditor::setAdvancedVisible (bool visible, bool save)
{
    advancedExpanded = visible;

    for (auto* knob : knobRowB)
        knob->setVisible (visible);

    advancedToggle.setButtonText ("Advanced");
    advancedToggle.setToggleState (visible, juce::dontSendNotification);

    resized();
    repaint();

    if (save)
        saveAdvancedState (visible);
}

void OnyVerbEditor::setInsaneMode (bool enabled, bool save)
{
    insaneMode = enabled;
    insaneModeButton.setToggleState (enabled, juce::dontSendNotification);
    particleOverlay.setInsaneMode (enabled);

    if (save)
        saveInsaneState (enabled);
}

void OnyVerbEditor::captureCompareSlot (char slot)
{
    auto state = onyProcessor.apvts.copyState();
    (slot == 'A' ? compareSlotA : compareSlotB) = state;
}

void OnyVerbEditor::applyCompareSlot (char slot)
{
    if (slot == activeCompareSlot)
        return;

    // Snapshot whichever slot we're leaving before switching, so A/B keeps
    // both sides live rather than only ever restoring a stale first capture.
    captureCompareSlot (activeCompareSlot);
    activeCompareSlot = slot;

    auto& target = (slot == 'A' ? compareSlotA : compareSlotB);
    if (target.isValid())
        onyProcessor.apvts.replaceState (target.createCopy());
}

} // namespace onyverb
