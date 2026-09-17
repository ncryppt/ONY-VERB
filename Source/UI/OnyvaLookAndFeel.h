#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include <functional>

namespace onyverb::ui
{

/** Custom-drawn controls throughout ONY Verb: rotary knobs with a glow ring
    that tracks value, vertical gain rails, the horizontal Character/Diffusion
    slider, and pill-style tab buttons for the mode selector. Deliberately not
    the stock JUCE look — this is the whole "boutique, not a demo" ask. */
class OnyvaLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    OnyvaLookAndFeel() : knobGrainTexture (makeGrainTexture (64)) { refreshColours(); }

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
        setColour (juce::PopupMenu::headerTextColourId, Theme::textSecondary);
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

        // Dotted position track instead of a solid arc — dots already
        // "passed" by the current value glow in the accent colour, the
        // rest sit dim. Reads as a more premium/hardware-inspired style
        // than a plain filled arc.
        constexpr int kNumTrackDots = 30;
        auto dotTrackRadius = radius * 0.92f;
        auto dotRadius = juce::jmax (1.1f, radius * 0.045f);
        for (int i = 0; i < kNumTrackDots; ++i)
        {
            auto t = (float) i / (float) (kNumTrackDots - 1);
            auto dotAngle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
            auto dotPos = centre.getPointOnCircumference (dotTrackRadius, dotAngle);

            if (dotAngle <= angle + 0.001f)
            {
                g.setColour (Theme::accent.withAlpha (0.3f));
                g.fillEllipse (juce::Rectangle<float> (dotRadius * 3.2f, dotRadius * 3.2f).withCentre (dotPos));
                g.setColour (Theme::accent);
                g.fillEllipse (juce::Rectangle<float> (dotRadius * 2.0f, dotRadius * 2.0f).withCentre (dotPos));
            }
            else
            {
                g.setColour (Theme::hairline);
                g.fillEllipse (juce::Rectangle<float> (dotRadius * 1.6f, dotRadius * 1.6f).withCentre (dotPos));
            }
        }

        // Knob body: a matte, textured finish rather than a glossy sphere —
        // a near-flat tone with only gentle top-lit shading, plus a faint
        // grain, instead of a strong directional gradient and a glossy
        // specular highlight.
        auto faceRadius = radius * 0.72f;

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (centre.x - faceRadius, centre.y - faceRadius + faceRadius * 0.12f, faceRadius * 2.0f, faceRadius * 2.0f);

        juce::ColourGradient faceGrad (Theme::panelRaised.brighter (0.05f), centre.x, centre.y - faceRadius,
                                        Theme::panel.darker (0.1f), centre.x, centre.y + faceRadius, false);
        g.setGradientFill (faceGrad);
        g.fillEllipse (centre.x - faceRadius, centre.y - faceRadius, faceRadius * 2.0f, faceRadius * 2.0f);

        auto faceBounds = juce::Rectangle<float> (faceRadius * 2.0f, faceRadius * 2.0f).withCentre (centre);
        stampGrain (g, [&] (juce::Path& p) { p.addEllipse (faceBounds); }, faceBounds);

        g.setColour (Theme::hairline);
        g.drawEllipse (centre.x - faceRadius, centre.y - faceRadius, faceRadius * 2.0f, faceRadius * 2.0f, 1.0f);

        // Rim progress arc: a solid glowing stroke traced directly along
        // the knob body's own edge, from the start of travel to the
        // current value — the same progress the dotted track outside
        // shows, but drawn right on the sphere itself. Colour shifts along
        // its length (dim at the low end, full accent at the current
        // value) using the theme's own accentDim/accent pair — the same
        // darker-to-brighter relationship the Character slider's track
        // already uses — so it re-colours with every theme rather than
        // being hardcoded to one hue.
        if (sliderPosProportional > 0.001f)
        {
            auto rimThickness = juce::jmax (2.0f, faceRadius * 0.09f);

            if (Theme::currentThemeIsLight)
            {
                // Sits just outside the knob's own edge and is clipped to
                // exclude the face entirely, so the light reads as peeking
                // out from behind the sphere — the "behind" look dark
                // themes get for free, since their glow colour blends into
                // the dark surface it overlaps rather than washing a tint
                // across a bright one.
                auto lightRimRadius = faceRadius + rimThickness * 0.5f;
                juce::Path rimArc;
                rimArc.addCentredArc (centre.x, centre.y, lightRimRadius, lightRimRadius, 0.0f, rotaryStartAngle, angle, true);

                auto arcStart = centre.getPointOnCircumference (lightRimRadius, rotaryStartAngle);
                auto arcEnd = centre.getPointOnCircumference (lightRimRadius, angle);

                juce::Path glowClip;
                glowClip.addRectangle (bounds.expanded (rimThickness * 4.0f));
                glowClip.addEllipse (centre.x - faceRadius, centre.y - faceRadius, faceRadius * 2.0f, faceRadius * 2.0f);
                glowClip.setUsingNonZeroWinding (false);

                g.saveState();
                g.reduceClipRegion (glowClip);

                juce::Path strokedRimArc;
                juce::PathStrokeType (rimThickness).createStrokedPath (strokedRimArc, rimArc);
                juce::DropShadow glow (Theme::accent.withAlpha (0.7f), (int) (rimThickness * 3.0f), {});
                glow.drawForPath (g, strokedRimArc);

                // Several progressively wider, more transparent strokes
                // stacked under a softened (not fully opaque) core, instead
                // of one hard-edged line — reads as a diffuse glowing line
                // rather than a sharp stroke.
                for (int i = 3; i >= 1; --i)
                {
                    auto w = rimThickness * (1.0f + (float) i * 0.5f);
                    auto alpha = 0.5f - (float) i * 0.1f;
                    juce::ColourGradient layerGrad (Theme::accentDim.withAlpha (alpha), arcStart.x, arcStart.y,
                                                      Theme::accent.withAlpha (alpha), arcEnd.x, arcEnd.y, false);
                    g.setGradientFill (layerGrad);
                    g.strokePath (rimArc, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }

                juce::ColourGradient coreGrad (Theme::accentDim.withAlpha (0.85f), arcStart.x, arcStart.y,
                                                 Theme::accent.withAlpha (0.85f), arcEnd.x, arcEnd.y, false);
                g.setGradientFill (coreGrad);
                g.strokePath (rimArc, juce::PathStrokeType (rimThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                g.restoreState();
            }
            else
            {
                auto rimRadius = faceRadius - 1.0f;
                juce::Path rimArc;
                rimArc.addCentredArc (centre.x, centre.y, rimRadius, rimRadius, 0.0f, rotaryStartAngle, angle, true);

                auto arcStart = centre.getPointOnCircumference (rimRadius, rotaryStartAngle);
                auto arcEnd = centre.getPointOnCircumference (rimRadius, angle);

                juce::ColourGradient glowGrad (Theme::accentDim.withAlpha (0.35f), arcStart.x, arcStart.y,
                                                 Theme::accent.withAlpha (0.35f), arcEnd.x, arcEnd.y, false);
                g.setGradientFill (glowGrad);
                g.strokePath (rimArc, juce::PathStrokeType (rimThickness * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                juce::ColourGradient rimGrad (Theme::accentDim, arcStart.x, arcStart.y,
                                                Theme::accent, arcEnd.x, arcEnd.y, false);
                g.setGradientFill (rimGrad);
                g.strokePath (rimArc, juce::PathStrokeType (rimThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }

        // Value indicator: a small glowing dot riding the rim of the body
        // instead of a pointer needle, matching the dotted track's language.
        auto indicatorPos = centre.getPointOnCircumference (faceRadius * 0.86f, angle);
        auto indicatorR = faceRadius * 0.14f;
        g.setColour (Theme::accent.withAlpha (0.4f));
        g.fillEllipse (juce::Rectangle<float> (indicatorR * 3.0f, indicatorR * 3.0f).withCentre (indicatorPos));
        g.setColour (Theme::accent);
        g.fillEllipse (juce::Rectangle<float> (indicatorR * 2.0f, indicatorR * 2.0f).withCentre (indicatorPos));
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.fillEllipse (juce::Rectangle<float> (indicatorR * 0.8f, indicatorR * 0.8f).withCentre (indicatorPos));

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

        if (isPill)
        {
            Theme::fillBeveledPill (g, bounds, button.getToggleState(), isMouseOverButton, isButtonDown);
            return;
        }

        auto radius = Theme::cornerRadius;
        Theme::dropShadowForRoundedRect (g, bounds, radius);

        if (button.getToggleState())
        {
            Theme::fillBeveledRoundedRect (g, bounds, radius, Theme::accent.withAlpha (0.16f));
            g.setColour (Theme::accent);
            g.drawRoundedRectangle (bounds, radius, 1.4f);
        }
        else
        {
            Theme::fillBeveledRoundedRect (g, bounds, radius, isMouseOverButton ? Theme::panelRaised.brighter (0.05f) : Theme::panelRaised);
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

    /** Value readouts (knobs, the Character/Decay sliders, the gain rails)
        opt into this via a "lcd" component property instead of the default
        plain-text label — a small backlit display chip in place of bare
        coloured text. */
    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        if (! label.getProperties().contains ("lcd"))
        {
            LookAndFeel_V4::drawLabel (g, label);
            return;
        }

        auto lit = static_cast<bool> (label.getProperties().getWithDefault ("lcdOn", true));

        auto bounds = label.getLocalBounds().toFloat();
        Theme::fillLcdChip (g, bounds, lit);

        // Lit text sits on a plain dark screen on dark themes (accent reads
        // fine there) but on the saturated accent wash a lit light-theme
        // chip gets instead, accent-on-accent has too little contrast —
        // white reads cleanly against that wash regardless of hue.
        auto litTextColour = Theme::currentThemeIsLight ? juce::Colours::white : Theme::accent;
        g.setColour (lit ? litTextColour : Theme::textDim);
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), bounds.getHeight() * 0.62f, juce::Font::bold)));
        g.drawFittedText (label.getText(), label.getLocalBounds(), juce::Justification::centred, 1);
    }

private:
    /** A small tileable grain: sparse salt-and-pepper specks (some lighter,
        some darker than whatever's underneath), clipped to each knob's own
        circle — reads as a matte/machined surface rather than static, since
        most of the tile stays fully transparent. Built once at construction
        and reused for every knob. */
    static juce::Image makeGrainTexture (int size)
    {
        juce::Image image (juce::Image::ARGB, size, size, true);
        juce::Random rng (1);

        for (int py = 0; py < size; ++py)
        {
            for (int px = 0; px < size; ++px)
            {
                auto roll = rng.nextFloat();
                if (roll > 0.4f)
                    continue;

                auto isLight = rng.nextBool();
                auto alpha = 0.03f + rng.nextFloat() * 0.09f;
                image.setPixelAt (px, py, (isLight ? juce::Colours::white : juce::Colours::black).withAlpha (alpha));
            }
        }

        return image;
    }

    juce::Image knobGrainTexture;

    void drawGainRail (juce::Graphics& g, juce::Rectangle<float> bounds,
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

        // Handle gets the same matte/grain finish as the rotary knobs —
        // gentle top-lit shading rather than a flat fill, plus the same
        // texture tile clipped to its shape.
        auto handleW = bounds.getWidth() * 0.85f;
        auto handleH = 14.0f;
        auto handle = juce::Rectangle<float> (0, 0, handleW, handleH).withCentre ({ bounds.getCentreX(), sliderPos });

        g.setColour (Theme::accent.withAlpha (0.3f));
        g.fillRoundedRectangle (handle.expanded (3.0f), handleH * 0.5f);

        juce::ColourGradient handleGrad (Theme::panelRaised.brighter (0.14f), handle.getX(), handle.getY(),
                                           Theme::panelRaised.darker (0.05f), handle.getX(), handle.getBottom(), false);
        g.setGradientFill (handleGrad);
        g.fillRoundedRectangle (handle, handleH * 0.5f);
        stampGrain (g, [&] (juce::Path& p) { p.addRoundedRectangle (handle, handleH * 0.5f); }, handle);

        g.setColour (Theme::accent);
        g.drawRoundedRectangle (handle, handleH * 0.5f, 1.4f);
    }

    void drawCharacterSliderTrack (juce::Graphics& g, juce::Rectangle<float> bounds,
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
        auto handleBounds = juce::Rectangle<float> (handleR * 2.0f, handleR * 2.0f).withCentre (handleCentre);

        g.setColour (Theme::accent.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (handleR * 2.6f, handleR * 2.6f).withCentre (handleCentre));

        juce::ColourGradient handleGrad (Theme::panelRaised.brighter (0.18f), handleCentre.x, handleBounds.getY(),
                                           Theme::panelRaised.darker (0.05f), handleCentre.x, handleBounds.getBottom(), false);
        g.setGradientFill (handleGrad);
        g.fillEllipse (handleBounds);
        stampGrain (g, [&] (juce::Path& p) { p.addEllipse (handleBounds); }, handleBounds);

        g.setColour (Theme::accent);
        g.drawEllipse (handleBounds, 1.6f);

        juce::ignoreUnused (minPos);
    }

    /** Clips to whatever shape `addShape` builds and stamps the shared
        grain texture into it — the common bit of drawGainRail()'s and
        drawCharacterSliderTrack()'s matte-handle finish. */
    void stampGrain (juce::Graphics& g, const std::function<void (juce::Path&)>& addShape, juce::Rectangle<float> fillBounds)
    {
        juce::Path shape;
        addShape (shape);
        g.saveState();
        g.reduceClipRegion (shape);
        g.setTiledImageFill (knobGrainTexture, 0, 0, 1.0f);
        g.fillRect (fillBounds);
        g.restoreState();
    }
};

} // namespace onyverb::ui
