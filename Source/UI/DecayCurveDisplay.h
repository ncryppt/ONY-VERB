#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "../Parameters.h"
#include "../DSP/VisualizationData.h"
#include <array>
#include <cmath>

namespace onyverb::ui
{

/** Top-strip display: a frequency-dependent decay-time (T60) curve, the
    same idea as an EQ curve panel but for "how long does each frequency
    band ring on" rather than gain. The curve itself is purely a function
    of the current parameter values (Size/Decay/Damping/Low+High Cut) — no
    audio data needed — smoothed so parameter changes animate rather than
    jump. A small Dry/Wet level meter pair lives in the same panel, driven
    by live audio via the ring buffer, so at a glance you can see both the
    shape of the tail *and* how much of each path is actually in the mix. */
class DecayCurveDisplay final : public juce::Component, private juce::Timer
{
public:
    DecayCurveDisplay (juce::AudioProcessorValueTreeState& state, dsp::VisualizationRingBuffer& ringBufferIn)
        : apvts (state), ringBuffer (ringBufferIn)
    {
        sizeParam    = apvts.getRawParameterValue (ParamIDs::size);
        decayParam   = apvts.getRawParameterValue (ParamIDs::decayTime);
        dampingParam = apvts.getRawParameterValue (ParamIDs::damping);
        lowCutParam  = apvts.getRawParameterValue (ParamIDs::lowCut);
        highCutParam = apvts.getRawParameterValue (ParamIDs::highCut);
        freezeParam  = apvts.getRawParameterValue (ParamIDs::freeze);

        for (auto& v : smoothedCurve) v = 0.0f;
        startTimerHz (30);
    }

    void setEcoMode (bool enabled) { startTimerHz (enabled ? 12 : 30); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (2.0f);

        Theme::dropShadowForRoundedRect (g, bounds, Theme::cornerRadius);
        Theme::fillBeveledRoundedRect (g, bounds, Theme::cornerRadius, Theme::panel);

        auto plot = bounds.reduced (14.0f, 10.0f);

        // Dry/Wet level meters carve a narrow strip off the right edge of
        // the panel — drawn after the curve/gridlines below so they sit on
        // top, but their area is reserved here first so the curve doesn't
        // stretch underneath them.
        auto meterStrip = plot.removeFromRight (26.0f);
        plot.removeFromRight (8.0f);

        // Faint horizontal gridlines.
        g.setColour (Theme::hairline.withAlpha (0.5f));
        for (int i = 1; i < 4; ++i)
        {
            auto y = plot.getY() + plot.getHeight() * (float) i / 4.0f;
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        }

        juce::Path curve;
        for (size_t i = 0; i < kNumPoints; ++i)
        {
            auto x = plot.getX() + plot.getWidth() * (float) i / (float) (kNumPoints - 1);
            auto y = plot.getBottom() - smoothedCurve[i] * plot.getHeight();
            if (i == 0) curve.startNewSubPath (x, y);
            else        curve.lineTo (x, y);
        }

        auto isFrozen = freezeParam != nullptr && freezeParam->load() > 0.5f;
        auto lineColour = isFrozen ? Theme::accent.brighter (0.3f) : Theme::accent;

        {
            juce::Path fill = curve;
            fill.lineTo (plot.getRight(), plot.getBottom());
            fill.lineTo (plot.getX(), plot.getBottom());
            fill.closeSubPath();
            g.setColour (lineColour.withAlpha (0.12f));
            g.fillPath (fill);
        }

        g.setColour (lineColour);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (Theme::textDim);
        g.setFont (Theme::labelFont (10.0f));
        g.drawText ("20Hz", (int) plot.getX(), (int) plot.getBottom() + 1, 50, 10, juce::Justification::left);
        g.drawText ("20kHz", (int) plot.getRight() - 50, (int) plot.getBottom() + 1, 50, 10, juce::Justification::right);

        // Dry/Wet level meters — live audio, not parameter-derived like the
        // curve above, so together they show both the shape of the tail and
        // how much of each path is actually reaching the output right now.
        auto drawMeter = [&] (juce::Rectangle<float> barArea, float levelNorm, juce::Colour colour, const char* label)
        {
            g.setColour (Theme::hairline);
            g.fillRoundedRectangle (barArea, 2.0f);

            auto fillHeight = barArea.getHeight() * juce::jlimit (0.0f, 1.0f, levelNorm);
            g.setColour (colour);
            g.fillRoundedRectangle (barArea.withTop (barArea.getBottom() - fillHeight), 2.0f);

            g.setColour (Theme::textDim);
            g.setFont (Theme::labelFont (9.0f));
            g.drawText (label, (int) barArea.getX() - 2, (int) barArea.getBottom() + 1, (int) barArea.getWidth() + 4, 10, juce::Justification::centred);
        };

        constexpr float barWidth = 8.0f, barGap = 3.0f;
        auto meterStartX = meterStrip.getX() + (meterStrip.getWidth() - (barWidth * 2.0f + barGap)) * 0.5f;
        juce::Rectangle<float> dryBar (meterStartX, meterStrip.getY(), barWidth, meterStrip.getHeight());
        juce::Rectangle<float> wetBar (meterStartX + barWidth + barGap, meterStrip.getY(), barWidth, meterStrip.getHeight());

        drawMeter (dryBar, meterResponse (smoothedDryLevel), Theme::textSecondary, "D");
        drawMeter (wetBar, meterResponse (smoothedWetLevel), lineColour, "W");
    }

private:
    static constexpr size_t kNumPoints = 48;

    // Perceptual-ish scaling so a typical program-level RMS reads usefully
    // on the meter instead of sitting near the bottom — this is a UI nicety,
    // not a calibrated metering standard.
    static float meterResponse (float level) { return juce::jlimit (0.0f, 1.0f, std::sqrt (level) * 1.4f); }

    static float freqAt (size_t i)
    {
        auto t = (float) i / (float) (kNumPoints - 1);
        return 20.0f * std::pow (1000.0f, t); // 20 Hz .. 20 kHz, log-spaced
    }

    void timerCallback() override
    {
        if (sizeParam == nullptr) return;

        auto sizeV = sizeParam->load();
        auto decayV = decayParam->load();
        auto dampingV = dampingParam->load();
        auto lowCutV = lowCutParam->load();
        auto highCutV = highCutParam->load();

        auto sizeMul = 0.9f + sizeV * 0.2f;

        for (size_t i = 0; i < kNumPoints; ++i)
        {
            auto f = freqAt (i);
            auto highShelf = juce::jlimit (0.0f, 1.0f, (std::log2 (f / 1000.0f)) / 4.5f + 0.15f);
            auto dampingAttenuation = 1.0f / (1.0f + dampingV * 3.0f * juce::jmax (0.0f, highShelf));

            auto lowCutAttenuation = f < lowCutV ? juce::jlimit (0.0f, 1.0f, f / juce::jmax (1.0f, lowCutV)) : 1.0f;
            auto highCutAttenuation = f > highCutV ? juce::jlimit (0.0f, 1.0f, highCutV / juce::jmax (1.0f, f)) : 1.0f;

            auto t60 = decayV * sizeMul * dampingAttenuation * lowCutAttenuation * highCutAttenuation;
            auto normalised = std::log1p (t60) / std::log1p (60.0f);

            targetCurve[i] = juce::jlimit (0.02f, 1.0f, normalised);
        }

        for (size_t i = 0; i < kNumPoints; ++i)
            smoothedCurve[i] += 0.18f * (targetCurve[i] - smoothedCurve[i]);

        // Dry/Wet meters: fast attack, slower release, like a real VU meter
        // — makes transients visible without the bars flickering on every
        // tiny fluctuation.
        static const dsp::VisualizationSnapshot silence {};
        auto snap = ringBuffer.popLatest (silence);
        auto approach = [] (float current, float target) { return current + (target > current ? 0.6f : 0.12f) * (target - current); };
        smoothedDryLevel = approach (smoothedDryLevel, snap.dryLevel);
        smoothedWetLevel = approach (smoothedWetLevel, snap.wetLevel);

        repaint();
    }

    juce::AudioProcessorValueTreeState& apvts;
    dsp::VisualizationRingBuffer& ringBuffer;
    std::atomic<float>* sizeParam = nullptr;
    std::atomic<float>* decayParam = nullptr;
    std::atomic<float>* dampingParam = nullptr;
    std::atomic<float>* lowCutParam = nullptr;
    std::atomic<float>* highCutParam = nullptr;
    std::atomic<float>* freezeParam = nullptr;

    std::array<float, kNumPoints> smoothedCurve;
    std::array<float, kNumPoints> targetCurve {};
    float smoothedDryLevel = 0.0f;
    float smoothedWetLevel = 0.0f;
};

} // namespace onyverb::ui
