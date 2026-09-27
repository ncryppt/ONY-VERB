#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "../Parameters.h"
#include "../DSP/VisualizationData.h"
#include "../DSP/SpectrumData.h"
#include <array>
#include <cmath>

namespace onyverb::ui
{

/** Top-strip display: a frequency-dependent decay-time (T60) curve, the
    same idea as an EQ curve panel but for "how long does each frequency
    band ring on" rather than gain. The curve itself is purely a function
    of the current parameter values (Size/Decay/Damping/Low+High Cut) — no
    audio data needed — smoothed so parameter changes animate rather than
    jump. A small Dry/Wet level meter pair, and live dry/wet frequency-
    spectrum traces (at the same 20Hz..20kHz log positions as the T60
    curve), live in the same panel, driven by real audio via the ring
    buffers — so at a glance you can see the tail's shape, how much of
    each path is in the mix, and where their actual energy sits relative
    to the Low/High Cut curve, all on one shared frequency axis.

    The Low Cut and High Cut points on the curve are draggable, Pro-Q-style
    — each is just a handle onto that knob's real parameter (horizontal
    drag only, sitting wherever the curve crosses that cutoff frequency)
    rather than an independent shaping node, so dragging one moves the
    matching knob and everything else (the knob's own value label, host
    automation, undo) follows for free through the normal APVTS parameter.
    The points themselves stay low-key and only appear while the pointer is
    over the panel (or an actual drag is in progress) — the rest of the
    time the display reads as a plain curve, not a control surface. */
class DecayCurveDisplay final : public juce::Component, private juce::Timer
{
public:
    DecayCurveDisplay (juce::AudioProcessorValueTreeState& state, dsp::VisualizationRingBuffer& ringBufferIn,
                        dsp::SpectrumRingBuffer& spectrumRingIn)
        : apvts (state), ringBuffer (ringBufferIn), spectrumRing (spectrumRingIn)
    {
        sizeParam    = apvts.getRawParameterValue (ParamIDs::size);
        decayParam   = apvts.getRawParameterValue (ParamIDs::decayTime);
        dampingParam = apvts.getRawParameterValue (ParamIDs::damping);
        lowCutParam  = apvts.getRawParameterValue (ParamIDs::lowCut);
        highCutParam = apvts.getRawParameterValue (ParamIDs::highCut);
        freezeParam  = apvts.getRawParameterValue (ParamIDs::freeze);

        lowCutRangedParam  = apvts.getParameter (ParamIDs::lowCut);
        highCutRangedParam = apvts.getParameter (ParamIDs::highCut);

        for (auto& v : smoothedCurve) v = 0.0f;
        startTimerHz (30);

        setWantsKeyboardFocus (false);
    }

    void setEcoMode (bool enabled) { startTimerHz (enabled ? 12 : 30); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (2.0f);

        Theme::dropShadowForRoundedRect (g, bounds, Theme::cornerRadius, 0.4f);
        Theme::fillBeveledRoundedRect (g, bounds, Theme::cornerRadius, Theme::panel);

        auto plot = computePlotBounds();

        // Dry/Wet level meters carve a narrow strip off the right edge of
        // the panel — reserved by computePlotBounds() so the curve doesn't
        // stretch underneath them; drawn separately below.
        auto meterStrip = bounds.reduced (14.0f, 10.0f).removeFromRight (26.0f);

        // Faint horizontal gridlines.
        g.setColour (Theme::hairline.withAlpha (0.5f));
        for (int i = 1; i < 4; ++i)
        {
            auto y = plot.getY() + plot.getHeight() * (float) i / 4.0f;
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        }

        auto isFrozen = freezeParam != nullptr && freezeParam->load() > 0.5f;
        auto lineColour = isFrozen ? Theme::accent.brighter (0.3f) : Theme::accent;

        // Dry/wet spectra as soft filled silhouettes, drawn before (so they
        // sit behind) the T60 curve — distinguished from it by shape/weight
        // rather than a new hue, matching this theme system's "one accent
        // colour" approach. Dry uses the same neutral tone as the "D" meter
        // bar; wet is accent-tinted like the "W" bar, but faint enough that
        // the crisp T60 line on top stays the primary read.
        auto drawSpectrumSilhouette = [&] (const std::array<float, kNumPoints>& spectrum, juce::Colour colour, float alpha)
        {
            juce::Path path;
            for (size_t i = 0; i < kNumPoints; ++i)
            {
                auto x = plot.getX() + plot.getWidth() * (float) i / (float) (kNumPoints - 1);
                auto y = plot.getBottom() - spectrum[i] * plot.getHeight();
                if (i == 0) path.startNewSubPath (x, y);
                else        path.lineTo (x, y);
            }
            path.lineTo (plot.getRight(), plot.getBottom());
            path.lineTo (plot.getX(), plot.getBottom());
            path.closeSubPath();

            g.setColour (colour.withAlpha (alpha));
            g.fillPath (path);
        };

        drawSpectrumSilhouette (smoothedDrySpectrum, Theme::textSecondary, 0.22f);
        drawSpectrumSilhouette (smoothedWetSpectrum, lineColour, 0.28f);

        juce::Path curve;
        for (size_t i = 0; i < kNumPoints; ++i)
        {
            auto x = plot.getX() + plot.getWidth() * (float) i / (float) (kNumPoints - 1);
            auto y = plot.getBottom() - smoothedCurve[i] * plot.getHeight();
            if (i == 0) curve.startNewSubPath (x, y);
            else        curve.lineTo (x, y);
        }

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

        drawHandles (g, plot, lineColour);

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

        // A glass sheen on top of everything else -- a raised pane of
        // glass reflects light in front of the curve/spectrum/meters
        // behind it, not behind them.
        Theme::drawGlassSheen (g, bounds, Theme::cornerRadius);

        // Corner screws, as if the glass itself were physically bolted to
        // the panel beneath it -- drawn on top of the sheen, since real
        // fasteners sit on the surface rather than behind the glass. Each
        // gets its own fixed slot angle so they don't all line up
        // identically, like they were actually hand-tightened.
        constexpr float screwRadius = 3.6f;
        constexpr float screwInset = 9.0f;
        Theme::drawScrew (g, { bounds.getX() + screwInset, bounds.getY() + screwInset }, screwRadius, juce::degreesToRadians (18.0f));
        Theme::drawScrew (g, { bounds.getRight() - screwInset, bounds.getY() + screwInset }, screwRadius, juce::degreesToRadians (-32.0f));
        Theme::drawScrew (g, { bounds.getX() + screwInset, bounds.getBottom() - screwInset }, screwRadius, juce::degreesToRadians (55.0f));
        Theme::drawScrew (g, { bounds.getRight() - screwInset, bounds.getBottom() - screwInset }, screwRadius, juce::degreesToRadians (-8.0f));
    }

    void mouseEnter (const juce::MouseEvent&) override
    {
        setPointerInside (true);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        setHoveredHandle (nearestHandle (e.position, computeHandlePositions (computePlotBounds())));
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        setPointerInside (false);
        setHoveredHandle (-1);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        activeHandle = nearestHandle (e.position, computeHandlePositions (computePlotBounds()));

        if (activeHandle < 0)
            return;

        if (auto* p = paramForHandle (activeHandle))
            p->beginChangeGesture();

        updateFromDrag (e.position, computePlotBounds());
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (activeHandle >= 0)
            updateFromDrag (e.position, computePlotBounds());
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (activeHandle < 0)
            return;

        if (auto* p = paramForHandle (activeHandle))
            p->endChangeGesture();

        activeHandle = -1;
        setHoveredHandle (-1);
    }

private:
    static constexpr size_t kNumPoints = 48;

    // Handle indices, in the order computeHandlePositions() fills them.
    enum HandleIndex { handleLowCut = 0, handleHighCut, handleCount };

    // Perceptual-ish scaling so a typical program-level RMS reads usefully
    // on the meter instead of sitting near the bottom — this is a UI nicety,
    // not a calibrated metering standard.
    static float meterResponse (float level) { return juce::jlimit (0.0f, 1.0f, std::sqrt (level) * 1.4f); }

    static float freqAt (size_t i)
    {
        auto t = (float) i / (float) (kNumPoints - 1);
        return 20.0f * std::pow (1000.0f, t); // 20 Hz .. 20 kHz, log-spaced
    }

    // Continuous versions of freqAt(), for the draggable handles (which
    // don't land on one of the kNumPoints sample indices).
    static float freqAtT (float t) { return 20.0f * std::pow (1000.0f, t); }
    static float tAtFreq (float f) { return std::log (juce::jmax (20.0f, f) / 20.0f) / std::log (1000.0f); }

    static float freqToX (float f, juce::Rectangle<float> plot) { return plot.getX() + tAtFreq (f) * plot.getWidth(); }
    static float xToFreq (float x, juce::Rectangle<float> plot) { return freqAtT (juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth())); }
    static float heightToY (float h, juce::Rectangle<float> plot) { return plot.getBottom() - h * plot.getHeight(); }
    static float yToHeight (float y, juce::Rectangle<float> plot) { return juce::jlimit (0.0f, 1.0f, (plot.getBottom() - y) / plot.getHeight()); }

    /** The same shape used by the drawn curve — factored out so the
        draggable handles can read (and, via the inverse functions below,
        write back to) exact points on it rather than only the 48 sampled
        curve vertices. */
    struct CurveParams { float sizeV, decayV, dampingV, lowCutV, highCutV; };

    static float highShelfAt (float f) { return juce::jlimit (0.0f, 1.0f, (std::log2 (f / 1000.0f)) / 4.5f + 0.15f); }

    static float computeNormalizedHeight (float f, const CurveParams& p)
    {
        auto sizeMul = 0.9f + p.sizeV * 0.2f;
        auto dampingAttenuation = 1.0f / (1.0f + p.dampingV * 3.0f * highShelfAt (f));
        auto lowCutAttenuation = f < p.lowCutV ? juce::jlimit (0.0f, 1.0f, f / juce::jmax (1.0f, p.lowCutV)) : 1.0f;
        auto highCutAttenuation = f > p.highCutV ? juce::jlimit (0.0f, 1.0f, p.highCutV / juce::jmax (1.0f, f)) : 1.0f;
        auto t60 = p.decayV * sizeMul * dampingAttenuation * lowCutAttenuation * highCutAttenuation;
        return juce::jlimit (0.02f, 1.0f, std::log1p (t60) / std::log1p (60.0f));
    }

    juce::Rectangle<float> computePlotBounds() const
    {
        auto bounds = getLocalBounds().toFloat().reduced (2.0f);
        auto plot = bounds.reduced (14.0f, 10.0f);
        plot.removeFromRight (26.0f + 8.0f); // the Dry/Wet meter strip and its gap
        return plot;
    }

    CurveParams currentParams() const
    {
        return { sizeParam->load(), decayParam->load(), dampingParam->load(), lowCutParam->load(), highCutParam->load() };
    }

    std::array<juce::Point<float>, handleCount> computeHandlePositions (juce::Rectangle<float> plot) const
    {
        auto p = currentParams();
        std::array<juce::Point<float>, handleCount> pts;
        pts[handleLowCut]  = { freqToX (p.lowCutV, plot),        heightToY (computeNormalizedHeight (p.lowCutV, p), plot) };
        pts[handleHighCut] = { freqToX (p.highCutV, plot),       heightToY (computeNormalizedHeight (p.highCutV, p), plot) };
        return pts;
    }

    static int nearestHandle (juce::Point<float> pos, const std::array<juce::Point<float>, handleCount>& pts)
    {
        constexpr float hitRadius = 14.0f;
        int best = -1;
        auto bestDist = hitRadius;

        for (int i = 0; i < (int) handleCount; ++i)
        {
            auto d = pos.getDistanceFrom (pts[(size_t) i]);
            if (d < bestDist) { bestDist = d; best = i; }
        }

        return best;
    }

    juce::RangedAudioParameter* paramForHandle (int handle) const
    {
        switch (handle)
        {
            case handleLowCut:  return lowCutRangedParam;
            case handleHighCut: return highCutRangedParam;
            default:            return nullptr;
        }
    }

    static void setParam (juce::RangedAudioParameter* p, float actualValue)
    {
        if (p != nullptr)
            p->setValueNotifyingHost (p->convertTo0to1 (actualValue));
    }

    void setHoveredHandle (int handle)
    {
        if (hoveredHandle == handle)
            return;

        hoveredHandle = handle;
        setMouseCursor (handle >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }

    void setPointerInside (bool inside)
    {
        if (pointerInside == inside)
            return;

        pointerInside = inside;
        repaint();
    }

    void updateFromDrag (juce::Point<float> pos, juce::Rectangle<float> plot)
    {
        auto p = currentParams();

        switch (activeHandle)
        {
            case handleLowCut:
            {
                // Clamped to the Low Cut parameter's own range (20-2000Hz,
                // see Parameters.h) as well as staying below High Cut —
                // the x-axis spans the full 20Hz-20kHz plot, which is wider
                // than Low Cut can actually reach.
                auto f = xToFreq (pos.x, plot);
                auto upperLimit = juce::jmin (2000.0f, juce::jmax (21.0f, p.highCutV * 0.9f));
                f = juce::jlimit (20.0f, upperLimit, f);
                setParam (lowCutRangedParam, f);
                break;
            }
            case handleHighCut:
            {
                // Same idea, clamped to High Cut's own 200-20000Hz range.
                auto f = xToFreq (pos.x, plot);
                auto lowerLimit = juce::jmax (200.0f, juce::jmin (19999.0f, p.lowCutV * 1.1f));
                f = juce::jlimit (lowerLimit, 20000.0f, f);
                setParam (highCutRangedParam, f);
                break;
            }
            default:
                break;
        }

        repaint();
    }

    void drawHandles (juce::Graphics& g, juce::Rectangle<float> plot, juce::Colour lineColour) const
    {
        // Kept out of sight until there's a reason to look for them — an
        // active drag, or the pointer sitting over the panel at all — so
        // the display normally reads as a plain curve rather than a control
        // surface asking to be clicked.
        if (! pointerInside && activeHandle < 0)
            return;

        auto pts = computeHandlePositions (plot);

        for (int i = 0; i < (int) handleCount; ++i)
        {
            auto isActive = activeHandle == i;
            auto isHovered = hoveredHandle == i;
            auto radius = isActive ? 4.2f : isHovered ? 3.6f : 2.4f;
            auto centre = pts[(size_t) i];

            if (isActive || isHovered)
            {
                g.setColour (lineColour.withAlpha (0.16f));
                g.fillEllipse (juce::Rectangle<float> (radius * 3.0f, radius * 3.0f).withCentre (centre));
            }

            g.setColour (Theme::panel.withAlpha (0.8f));
            g.fillEllipse (juce::Rectangle<float> (radius * 2.0f + 1.5f, radius * 2.0f + 1.5f).withCentre (centre));
            g.setColour (lineColour.withAlpha (isActive || isHovered ? 0.9f : 0.5f));
            g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
        }
    }

    void timerCallback() override
    {
        if (sizeParam == nullptr) return;

        auto p = currentParams();

        for (size_t i = 0; i < kNumPoints; ++i)
            targetCurve[i] = computeNormalizedHeight (freqAt (i), p);

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

        // Spectra only actually update once per FFT window (~93ms), so a
        // plain EMA here is enough to keep the trace from stepping visibly
        // between updates without needing VU-style attack/release.
        static const dsp::SpectrumSnapshot silentSpectrum {};
        auto spectrumSnap = spectrumRing.popLatest (silentSpectrum);
        for (size_t i = 0; i < kNumPoints; ++i)
        {
            smoothedDrySpectrum[i] += 0.25f * (spectrumSnap.dry[i] - smoothedDrySpectrum[i]);
            smoothedWetSpectrum[i] += 0.25f * (spectrumSnap.wet[i] - smoothedWetSpectrum[i]);
        }

        repaint();
    }

    juce::AudioProcessorValueTreeState& apvts;
    dsp::VisualizationRingBuffer& ringBuffer;
    dsp::SpectrumRingBuffer& spectrumRing;
    std::atomic<float>* sizeParam = nullptr;
    std::atomic<float>* decayParam = nullptr;
    std::atomic<float>* dampingParam = nullptr;
    std::atomic<float>* lowCutParam = nullptr;
    std::atomic<float>* highCutParam = nullptr;
    std::atomic<float>* freezeParam = nullptr;

    // Real parameter objects (rather than the raw atomics above) so
    // dragging a handle can write back through the normal
    // begin/setValueNotifyingHost/endChangeGesture path — the same one
    // SliderParameterAttachment uses — which keeps host automation, undo,
    // and the knob's own slider/label all in sync for free.
    juce::RangedAudioParameter* lowCutRangedParam = nullptr;
    juce::RangedAudioParameter* highCutRangedParam = nullptr;

    int activeHandle = -1;
    int hoveredHandle = -1;
    bool pointerInside = false;

    std::array<float, kNumPoints> smoothedCurve;
    std::array<float, kNumPoints> targetCurve {};
    float smoothedDryLevel = 0.0f;
    float smoothedWetLevel = 0.0f;
    std::array<float, kNumPoints> smoothedDrySpectrum {};
    std::array<float, kNumPoints> smoothedWetSpectrum {};
};

} // namespace onyverb::ui
