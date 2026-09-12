#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "../DSP/VisualizationData.h"

namespace onyverb::ui
{

/** Small, unobtrusive stereo correlation meter: -1 (out of phase) to +1
    (mono-identical), needle-style. Meant to sit quietly in a bottom corner,
    not compete with the orb. */
class CorrelationMeter final : public juce::Component, private juce::Timer
{
public:
    explicit CorrelationMeter (dsp::VisualizationRingBuffer& ringBufferIn) : ringBuffer (ringBufferIn)
    {
        startTimerHz (24);
    }

    void setEcoMode (bool enabled) { startTimerHz (enabled ? 10 : 24); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour (Theme::panel);
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (Theme::hairline);
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

        auto track = bounds.reduced (6.0f, bounds.getHeight() * 0.5f - 2.0f);
        g.setColour (Theme::hairline);
        g.fillRoundedRectangle (track, 2.0f);

        auto midX = track.getCentreX();
        auto pos = midX + (track.getWidth() * 0.5f) * smoothedCorrelation;

        g.setColour (Theme::accent);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ pos, track.getCentreY() }));

        g.setColour (Theme::textDim);
        g.setFont (Theme::labelFont (9.0f));
        g.drawText ("L/R", bounds.removeFromTop (10.0f), juce::Justification::centred);
    }

private:
    void timerCallback() override
    {
        static const dsp::VisualizationSnapshot silence {};
        auto snap = ringBuffer.popLatest (silence);
        smoothedCorrelation += 0.2f * (snap.correlation - smoothedCorrelation);
        repaint();
    }

    dsp::VisualizationRingBuffer& ringBuffer;
    float smoothedCorrelation = 0.0f;
};

} // namespace onyverb::ui
