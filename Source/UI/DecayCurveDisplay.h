#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "../Parameters.h"
#include <array>
#include <cmath>

namespace onyverb::ui
{

/** Top-strip display: a frequency-dependent decay-time (T60) curve, the
    same idea as an EQ curve panel but for "how long does each frequency
    band ring on" rather than gain. Purely a function of the current
    parameter values (Size/Decay/Damping/Low+High Cut) — no audio data
    needed — smoothed so parameter changes animate rather than jump. */
class DecayCurveDisplay final : public juce::Component, private juce::Timer
{
public:
    explicit DecayCurveDisplay (juce::AudioProcessorValueTreeState& state) : apvts (state)
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

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (2.0f);

        g.setColour (Theme::panel);
        g.fillRoundedRectangle (bounds, Theme::cornerRadius);
        g.setColour (Theme::hairline);
        g.drawRoundedRectangle (bounds, Theme::cornerRadius, 1.0f);

        auto plot = bounds.reduced (14.0f, 10.0f);

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
    }

private:
    static constexpr size_t kNumPoints = 48;

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

        bool changed = false;
        for (size_t i = 0; i < kNumPoints; ++i)
        {
            auto prev = smoothedCurve[i];
            smoothedCurve[i] += 0.18f * (targetCurve[i] - smoothedCurve[i]);
            if (std::abs (smoothedCurve[i] - prev) > 0.0005f)
                changed = true;
        }

        if (changed)
            repaint();
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::atomic<float>* sizeParam = nullptr;
    std::atomic<float>* decayParam = nullptr;
    std::atomic<float>* dampingParam = nullptr;
    std::atomic<float>* lowCutParam = nullptr;
    std::atomic<float>* highCutParam = nullptr;
    std::atomic<float>* freezeParam = nullptr;

    std::array<float, kNumPoints> smoothedCurve;
    std::array<float, kNumPoints> targetCurve {};
};

} // namespace onyverb::ui
