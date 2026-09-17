#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "../DSP/VisualizationData.h"
#include <array>
#include <cmath>

namespace onyverb::ui
{

/** The hero element: a glowing orb driven by the actual reverb tail energy
    and output level, not a decorative animation. Reads snapshots pushed by
    the audio thread via the processor's lock-free ring buffer and smooths
    them itself for animation (the ring buffer can deliver several snapshots
    per repaint tick; we only need the latest, smoothed).

    Rendering is a few layered passes rather than one flat fill: a single
    continuous radial gradient for the glow+body (smoother falloff than
    stacked flat-alpha circles), a glassy specular highlight to read as a
    sphere rather than a disc, a slow segmented "HUD" ring that's always
    faintly present, and short-lived expanding pulse rings triggered by
    actual transients in the signal (a snare hit visibly pings the orb). */
class OrbVisualizer final : public juce::Component, private juce::Timer
{
public:
    OrbVisualizer (dsp::VisualizationRingBuffer& ringBufferIn, std::atomic<float>* freezeParamIn)
        : ringBuffer (ringBufferIn), freezeParam (freezeParamIn)
    {
        pulseAges.fill (-1.0f);
        buildMesh();
        startTimerHz (45);
    }

    /** For slower/older machines: halves the repaint rate rather than
        stripping visual features, since the timer tick rate is the main
        CPU cost here. */
    void setEcoMode (bool enabled)
    {
        auto hz = enabled ? 20 : 45;
        startTimerHz (hz);
        frameSeconds = 1.0f / (float) hz;
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto centre = bounds.getCentre();
        auto maxRadius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;

        auto breathing = 0.5f + 0.5f * std::sin (idlePhase);
        auto liveliness = currentLiveliness;
        auto radius = maxRadius * (0.5f + 0.4f * liveliness + 0.06f * breathing);

        if (Theme::acidTripActive)
        {
            // Multi-frequency "melt" wobble instead of a clean circle — the
            // base radius already rainbow-cycles colour via Theme::accent,
            // this makes the shape itself feel unstable too.
            radius *= 1.0f + 0.05f * std::sin (idlePhase * 2.7f) + 0.035f * std::sin (idlePhase * 4.3f + 1.3f);
        }

        // Tracks whichever theme is active (hue and saturation both come
        // from Theme::accent) rather than a hardcoded blue, with the same
        // small live hue drift as before layered on top.
        auto accentHue = Theme::accent.getHue();
        auto accentSat = Theme::accent.getSaturation();
        auto hue = accentHue + brightness * 0.025f;
        auto coreColour = juce::Colour::fromHSV (hue, juce::jlimit (0.0f, 1.0f, accentSat * 0.6f), 1.0f, 1.0f);
        auto glowColour = juce::Colour::fromHSV (hue, juce::jlimit (0.0f, 1.0f, accentSat + 0.15f), 1.0f, 1.0f);

        drawGlowAndBody (g, centre, radius, maxRadius, coreColour, glowColour, liveliness);
        drawSpecularHighlight (g, centre, radius);
        drawWireframeMesh (g, centre, radius, maxRadius, glowColour, liveliness);

        drawHudRing (g, centre, radius, maxRadius, glowColour);
        drawPulses (g, centre, radius, maxRadius, glowColour);

        if (Theme::acidTripActive)
            drawAcidSwirl (g, centre, radius, maxRadius);
    }

    /** Current 0..1 "how energetic does the orb look right now" value, for
        other components (the particle overlay) that want to react the same
        way the orb does without touching the ring buffer themselves — it's
        single-producer/single-consumer, so a second consumer draining it
        independently would just steal snapshots from this one. */
    float getLiveliness() const noexcept { return currentLiveliness; }

    /** Fires whenever a transient pings the orb's own pulse rings, so
        something else (the particle overlay) can burst in sync rather than
        running its own separate transient detector on the same data. */
    std::function<void()> onTransient;

private:
    // The component itself is a square sized to `maxRadius`, and JUCE clips
    // all painting to a component's own bounds — so any glow/ring drawn
    // past maxRadius gets hard-clipped into a visible square around the
    // (otherwise circular) orb. Every radius below is kept just inside that
    // limit so everything fades away (or simply ends) before the edge.
    static float capRadius (float r, float maxRadius) { return juce::jmin (r, maxRadius * 0.985f); }

    // ---------------------------------------------------------------------
    // Wireframe mesh: a UV-sphere grid of points, displaced along their own
    // radius by a smooth (not per-point-random) noise field so neighbouring
    // points bulge and fold together into lumpy, organic-looking blobs
    // rather than a spiky sea urchin, then spun live and projected with a
    // simple orthographic rotation — a 2D approximation of a genuinely 3D
    // audio-reactive mesh rather than a flat disc.
    // ---------------------------------------------------------------------

    struct MeshPoint { float x, y, z, theta, phi; };

    static constexpr int kLatRings = 11;
    static constexpr int kLonSegments = 18;
    static constexpr int kNumMeshPoints = (kLatRings + 1) * kLonSegments;
    static constexpr float kViewTilt = -0.5f; // fixed viewing angle so the grid's curvature reads as 3D rather than face-on

    void buildMesh()
    {
        int idx = 0;
        for (int i = 0; i <= kLatRings; ++i)
        {
            auto theta = (float) i / (float) kLatRings * juce::MathConstants<float>::pi;

            for (int j = 0; j < kLonSegments; ++j)
            {
                auto phi = (float) j / (float) kLonSegments * juce::MathConstants<float>::twoPi;

                meshPoints[(size_t) idx++] = {
                    std::sin (theta) * std::cos (phi),
                    std::cos (theta),
                    std::sin (theta) * std::sin (phi),
                    theta, phi
                };
            }
        }
    }

    /** A handful of low-frequency sinusoids summed together — cheap next to
        real 3D noise, but continuous across the sphere's surface (unlike
        independent per-vertex randomness), which is what actually reads as
        smooth lumps and creases instead of static-y spikes. Roughly in
        [-1, 1]. `time` slowly drifts the pattern so the blob subtly writhes
        even at rest. */
    static float blobNoise (float theta, float phi, float time)
    {
        float n = 0.0f;
        n += 0.35f * std::sin (3.0f * theta + time * 0.6f) * std::cos (2.0f * phi + time * 0.4f);
        n += 0.25f * std::sin (5.0f * theta - time * 0.5f + 1.7f) * std::cos (4.0f * phi + 2.1f);
        n += 0.22f * std::sin (2.0f * theta + 0.8f) * std::cos (6.0f * phi - time * 0.7f + 0.4f);
        n += 0.18f * std::sin (7.0f * theta - 0.6f) * std::cos (3.0f * phi + time * 0.3f + 3.0f);
        return n;
    }

    void drawWireframeMesh (juce::Graphics& g, juce::Point<float> centre, float radius, float maxRadius,
                             juce::Colour glowColour, float liveliness) const
    {
        auto spin = meshSpin;
        auto cosSpin = std::cos (spin), sinSpin = std::sin (spin);
        auto cosTilt = std::cos (kViewTilt), sinTilt = std::sin (kViewTilt);
        auto bulgeAmount = 0.26f + 0.22f * liveliness;
        auto r = capRadius (radius, maxRadius);

        std::array<juce::Point<float>, kNumMeshPoints> projected;
        std::array<float, kNumMeshPoints> depth;

        for (int idx = 0; idx < kNumMeshPoints; ++idx)
        {
            auto& p = meshPoints[(size_t) idx];
            auto bulge = 1.0f + bulgeAmount * blobNoise (p.theta, p.phi, meshNoisePhase);

            auto x = p.x * bulge, y = p.y * bulge, z = p.z * bulge;

            // Fixed tilt (around X) for a pleasant viewing angle, then the
            // live spin (around Y) on top of it.
            auto ty = y * cosTilt - z * sinTilt;
            auto tz = y * sinTilt + z * cosTilt;
            auto rx = x * cosSpin - tz * sinSpin;
            auto rz = x * sinSpin + tz * cosSpin;

            projected[(size_t) idx] = { centre.x + rx * r, centre.y + ty * r };
            depth[(size_t) idx] = rz;
        }

        auto edgeAlpha = [] (float d) { return juce::jmap (juce::jlimit (-1.0f, 1.0f, d), -1.0f, 1.0f, 0.06f, 0.8f); };

        for (int i = 0; i <= kLatRings; ++i)
        {
            for (int j = 0; j < kLonSegments; ++j)
            {
                auto idx = i * kLonSegments + j;
                auto ringNeighbour = i * kLonSegments + (j + 1) % kLonSegments;

                auto a = edgeAlpha (depth[(size_t) idx]);
                auto b = edgeAlpha (depth[(size_t) ringNeighbour]);
                g.setColour (glowColour.withAlpha (juce::jmax (a, b) * 0.65f));
                g.drawLine (juce::Line<float> (projected[(size_t) idx], projected[(size_t) ringNeighbour]), 0.9f);

                if (i < kLatRings)
                {
                    auto belowNeighbour = (i + 1) * kLonSegments + j;
                    auto c = edgeAlpha (depth[(size_t) belowNeighbour]);
                    g.setColour (glowColour.withAlpha (juce::jmax (a, c) * 0.65f));
                    g.drawLine (juce::Line<float> (projected[(size_t) idx], projected[(size_t) belowNeighbour]), 0.9f);
                }
            }
        }

        for (int idx = 0; idx < kNumMeshPoints; ++idx)
        {
            auto a = edgeAlpha (depth[(size_t) idx]);
            auto dotR = 1.4f;
            g.setColour (glowColour.withAlpha (a));
            g.fillEllipse (juce::Rectangle<float> (dotR * 2.0f, dotR * 2.0f).withCentre (projected[(size_t) idx]));
        }
    }

    void drawGlowAndBody (juce::Graphics& g, juce::Point<float> centre, float radius, float maxRadius,
                           juce::Colour coreColour, juce::Colour glowColour, float liveliness) const
    {
        auto outerRadius = capRadius (radius * 2.7f, maxRadius);
        auto glowStrength = 0.4f + 0.6f * liveliness;

        juce::ColourGradient body (juce::Colours::white.withAlpha (0.95f), centre.x, centre.y,
                                    glowColour.withAlpha (0.0f), centre.x + outerRadius, centre.y, true);
        body.addColour (0.10, juce::Colours::white.withAlpha (0.88f));
        body.addColour (0.26, coreColour.withAlpha (0.92f));
        body.addColour (0.42, glowColour.withAlpha (0.7f * glowStrength));
        body.addColour (0.62, glowColour.withAlpha (0.32f * glowStrength));
        body.addColour (0.85, glowColour.withAlpha (0.08f * glowStrength));

        g.setGradientFill (body);
        g.fillEllipse (juce::Rectangle<float> (outerRadius * 2.0f, outerRadius * 2.0f).withCentre (centre));
    }

    static void drawSpecularHighlight (juce::Graphics& g, juce::Point<float> centre, float radius)
    {
        auto highlightCentre = centre.translated (-radius * 0.3f, -radius * 0.34f);
        auto highlightRadius = radius * 0.55f;

        juce::ColourGradient highlight (juce::Colours::white.withAlpha (0.5f), highlightCentre.x, highlightCentre.y,
                                         juce::Colours::white.withAlpha (0.0f), highlightCentre.x + highlightRadius, highlightCentre.y, true);
        g.setGradientFill (highlight);
        g.fillEllipse (juce::Rectangle<float> (highlightRadius * 2.0f, highlightRadius * 2.0f).withCentre (highlightCentre));
    }

    void drawHudRing (juce::Graphics& g, juce::Point<float> centre, float radius, float maxRadius, juce::Colour glowColour) const
    {
        auto ringRadius = capRadius (radius * (freezeActive ? 1.34f : 1.2f), maxRadius);
        auto numSegments = 18;
        auto segmentSpan = juce::MathConstants<float>::twoPi / (float) numSegments * 0.55f;
        auto alpha = freezeActive ? 0.6f : 0.16f + 0.1f * (0.5f + 0.5f * std::sin (idlePhase * 0.6f));

        g.setColour (glowColour.withAlpha (alpha));
        for (int i = 0; i < numSegments; ++i)
        {
            auto start = ringAngle + (float) i / (float) numSegments * juce::MathConstants<float>::twoPi;
            juce::Path seg;
            seg.addCentredArc (centre.x, centre.y, ringRadius, ringRadius, 0.0f, start, start + segmentSpan, true);
            g.strokePath (seg, juce::PathStrokeType (freezeActive ? 2.0f : 1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        if (freezeActive)
        {
            // A second, slower contra-rotating ring makes "held" visually
            // distinct from "decaying", rather than just brighter.
            auto counterRadius = capRadius (radius * 1.5f, maxRadius);
            juce::Path counterRing;
            counterRing.addCentredArc (centre.x, centre.y, counterRadius, counterRadius, 0.0f,
                                        -ringAngle * 0.6f, -ringAngle * 0.6f + juce::MathConstants<float>::pi * 1.15f, true);
            g.setColour (Theme::accent.withAlpha (0.35f));
            g.strokePath (counterRing, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    void drawAcidSwirl (juce::Graphics& g, juce::Point<float> centre, float radius, float maxRadius) const
    {
        // Three rings, each a different point on the colour wheel from the
        // (already rainbow-cycling) accent hue, each rotating at its own
        // rate/direction — a kaleidoscope halo on top of the base glow.
        auto baseHue = Theme::accent.getHue();
        static const float offsets[] = { 0.0f, 0.33f, 0.66f };
        static const float speeds[]  = { 1.0f, -1.6f, 2.3f };
        static const float radii[]   = { 1.15f, 1.45f, 1.75f };

        for (int i = 0; i < 3; ++i)
        {
            auto ringRadius = capRadius (radius * radii[i], maxRadius);
            auto hue = std::fmod (baseHue + offsets[i] + 1.0f, 1.0f);
            auto colour = juce::Colour::fromHSV (hue, 0.9f, 1.0f, 1.0f);
            auto angle = swirlPhase * speeds[i];

            juce::Path ring;
            ring.addCentredArc (centre.x, centre.y, ringRadius, ringRadius, 0.0f,
                                 angle, angle + juce::MathConstants<float>::pi * 1.4f, true);
            g.setColour (colour.withAlpha (0.4f));
            g.strokePath (ring, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    void drawPulses (juce::Graphics& g, juce::Point<float> centre, float baseRadius, float maxRadius, juce::Colour glowColour) const
    {
        for (auto age : pulseAges)
        {
            if (age < 0.0f) continue;
            auto t = age / kPulseLifetime;
            auto r = capRadius (baseRadius * (1.05f + t * 0.85f), maxRadius);
            auto alpha = (1.0f - t) * 0.45f;

            g.setColour (glowColour.withAlpha (alpha));
            g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre), 1.8f * (1.0f - t * 0.6f));
        }
    }

    void timerCallback() override
    {
        freezeActive = freezeParam != nullptr && freezeParam->load() > 0.5f;

        static const dsp::VisualizationSnapshot silence {};
        auto snap = ringBuffer.popLatest (silence);

        auto follow = [] (float& state, float target, float coeff)
        {
            state += coeff * (target - state);
        };

        auto prevTail = smoothedTail;
        follow (smoothedLevel, snap.outputLevel, 0.35f);
        follow (smoothedTail, snap.tailEnergy, 0.2f);
        follow (brightness, snap.brightness, 0.1f);

        auto breathingNow = 0.5f + 0.5f * std::sin (idlePhase);
        auto energy = juce::jlimit (0.0f, 1.0f, smoothedTail * 3.0f + smoothedLevel * 1.5f);
        currentLiveliness = freezeActive ? juce::jmax (energy, 0.55f + 0.1f * breathingNow) : energy;

        // Transient detection: a fast rise in tail energy pings a pulse ring.
        if (smoothedTail - prevTail > kPulseTriggerDelta && pulseCooldown <= 0.0f)
        {
            spawnPulse();
            if (onTransient) onTransient();
            pulseCooldown = 0.12f;
        }
        pulseCooldown = juce::jmax (0.0f, pulseCooldown - frameSeconds);

        for (auto& age : pulseAges)
            if (age >= 0.0f)
                age = (age + frameSeconds > kPulseLifetime) ? -1.0f : age + frameSeconds;

        idlePhase += 0.06f;
        if (idlePhase > juce::MathConstants<float>::twoPi)
            idlePhase -= juce::MathConstants<float>::twoPi;

        ringAngle += freezeActive ? 0.012f : 0.006f;
        if (ringAngle > juce::MathConstants<float>::twoPi)
            ringAngle -= juce::MathConstants<float>::twoPi;

        swirlPhase += 0.025f;
        if (swirlPhase > juce::MathConstants<float>::twoPi)
            swirlPhase -= juce::MathConstants<float>::twoPi;

        meshSpin += 0.011f;
        if (meshSpin > juce::MathConstants<float>::twoPi)
            meshSpin -= juce::MathConstants<float>::twoPi;

        meshNoisePhase += frameSeconds * 0.7f;

        repaint();
    }

    void spawnPulse()
    {
        for (auto& age : pulseAges)
        {
            if (age < 0.0f)
            {
                age = 0.0f;
                return;
            }
        }
    }

    static constexpr float kPulseLifetime = 0.9f;
    static constexpr float kPulseTriggerDelta = 0.035f;
    float frameSeconds = 1.0f / 45.0f;

    dsp::VisualizationRingBuffer& ringBuffer;
    std::atomic<float>* freezeParam = nullptr;
    float smoothedLevel = 0.0f, smoothedTail = 0.0f, brightness = 0.5f;
    float currentLiveliness = 0.0f;
    float idlePhase = 0.0f;
    float ringAngle = 0.0f;
    float swirlPhase = 0.0f;
    float pulseCooldown = 0.0f;
    std::array<float, 4> pulseAges;
    bool freezeActive = false;

    std::array<MeshPoint, kNumMeshPoints> meshPoints;
    float meshSpin = 0.0f;
    float meshNoisePhase = 0.0f;
};

} // namespace onyverb::ui
