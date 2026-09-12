#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "LeafShape.h"
#include <array>
#include <cmath>
#include <functional>

namespace onyverb::ui
{

/** A full-window overlay (sized to the whole editor, sitting on top of
    everything, click-through) that shoots small glowing particles out from
    the orb's position. Ambient emission rate tracks how energetic the orb
    currently looks (via OrbVisualizer::getLiveliness(), not the ring buffer
    directly — see that method's comment for why), and `spawnBurst()` — wired
    to OrbVisualizer::onTransient — fires an energetic burst in sync with the
    same transients that ping the orb's own pulse rings.

    When the "Kush Koma" theme is active, the glow dots become small
    tumbling leaf shapes and a steady wisp of smoke drifts up from the orb
    regardless of loudness — a themed variant rather than a second overlay,
    since it's the same pool/timer/culling machinery either way. */
class ParticleOverlay final : public juce::Component, private juce::Timer
{
public:
    ParticleOverlay()
    {
        setInterceptsMouseClicks (false, false);
        leafTemplate = makeLeafPath (kLeafTemplateSize);
        startTimerHz (45);
    }

    /** Called from the editor's resized() whenever the orb moves/resizes,
        so particles know where to originate (in this overlay's own
        coordinates, i.e. the editor's local space). */
    void setOrbGeometry (juce::Point<float> centre, float radius)
    {
        orbCentre = centre;
        orbRadius = radius;
    }

    void setLivelinessSource (std::function<float()> source) { livelinessSource = std::move (source); }

    /** Exaggerates everything — more particles, faster, bigger, brighter —
        rather than being a separate visual style. */
    void setInsaneMode (bool enabled) { insane = enabled; }

    /** For slower/older machines: fewer particles and a slower tick rate,
        since paint cost here scales directly with the active particle
        count — the main CPU driver of this whole overlay. */
    void setEcoMode (bool enabled)
    {
        eco = enabled;
        auto hz = enabled ? 20 : 45;
        startTimerHz (hz);
        frameSeconds = 1.0f / (float) hz;
    }

    void spawnBurst()
    {
        auto count = insane ? 55 : 16;
        if (eco) count /= 3;
        for (int i = 0; i < count; ++i)
            spawnParticle (true);
    }

    void paint (juce::Graphics& g) override
    {
        auto hue = Theme::accent.getHue();
        auto sat = juce::jlimit (0.0f, 1.0f, Theme::accent.getSaturation());

        for (auto& p : particles)
        {
            if (! p.active) continue;

            auto t = juce::jlimit (0.0f, 1.0f, p.age / p.life);

            if (p.kind == Particle::Kind::Smoke)
            {
                auto alpha = std::sin (t * juce::MathConstants<float>::pi) * p.baseAlpha;
                auto radius = p.size * (0.6f + t * 1.5f);
                g.setColour (Theme::textSecondary.withAlpha (alpha));
                g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (p.pos));
                continue;
            }

            auto alpha = (1.0f - t) * p.baseAlpha;

            // Acid Trip: each particle's hue keeps shifting as it ages
            // (a little rainbow trail) instead of sitting near one hue.
            auto particleHue = Theme::acidTripActive ? std::fmod (hue + p.hueOffset + t * 1.3f + 1.0f, 1.0f) : hue + p.hueOffset;
            auto particleSat = Theme::acidTripActive ? 1.0f : sat;

            if (p.kind == Particle::Kind::Leaf)
            {
                auto colour = juce::Colour::fromHSV (particleHue, particleSat, 1.0f, 1.0f);
                g.setColour (colour.withAlpha (alpha));
                auto scale = (p.size / kLeafTemplateSize) * (1.0f - t * 0.25f);
                g.fillPath (leafTemplate, juce::AffineTransform::scale (scale)
                                                                  .rotated (p.rotation)
                                                                  .translated (p.pos));
                continue;
            }

            auto radius = p.size * (1.0f - t * 0.35f);
            auto colour = juce::Colour::fromHSV (particleHue, particleSat, 1.0f, 1.0f);

            g.setColour (colour.withAlpha (alpha * 0.3f));
            g.fillEllipse (juce::Rectangle<float> (radius * 4.0f, radius * 4.0f).withCentre (p.pos));
            g.setColour (colour.withAlpha (alpha));
            g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (p.pos));
        }
    }

private:
    struct Particle
    {
        enum class Kind { Glow, Leaf, Smoke };

        juce::Point<float> pos, velocity;
        float age = 0.0f, life = 1.0f, size = 2.0f, hueOffset = 0.0f, baseAlpha = 0.8f;
        float rotation = 0.0f, rotationSpin = 0.0f;
        Kind kind = Kind::Glow;
        bool active = false;
    };

    void spawnParticle (bool burst)
    {
        for (auto& p : particles)
        {
            if (p.active) continue;

            auto speedMul = insane ? 2.2f : 1.0f;
            auto sizeMul = insane ? 1.8f : 1.0f;

            auto angle = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            juce::Point<float> direction (std::cos (angle), std::sin (angle));
            auto speed = (burst ? (150.0f + rng.nextFloat() * 190.0f) : (25.0f + rng.nextFloat() * 55.0f)) * speedMul;
            auto startRadius = orbRadius * (0.55f + rng.nextFloat() * 0.5f);

            p.pos = orbCentre + direction * startRadius;
            p.velocity = direction * speed;
            p.age = 0.0f;
            p.life = burst ? (1.0f + rng.nextFloat() * 0.7f) : (1.6f + rng.nextFloat() * 1.6f);
            p.size = (burst ? (1.8f + rng.nextFloat() * 1.8f) : (1.1f + rng.nextFloat() * 1.3f)) * sizeMul;
            p.hueOffset = Theme::acidTripActive ? rng.nextFloat() : (rng.nextFloat() * 2.0f - 1.0f) * (insane ? 0.09f : 0.05f);
            p.baseAlpha = (burst ? 0.9f : 0.6f) * (insane ? 1.1f : 1.0f);
            p.rotation = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            p.rotationSpin = (rng.nextFloat() * 2.0f - 1.0f) * 5.0f;

            if (Theme::kushKomaActive)
            {
                p.kind = Particle::Kind::Leaf;
                p.size *= 3.5f; // leaves need to be bigger than a glow dot to actually read as leaves
            }
            else
            {
                p.kind = Particle::Kind::Glow;
            }

            p.active = true;
            return;
        }
    }

    void spawnSmoke()
    {
        for (auto& p : particles)
        {
            if (p.active) continue;

            // Mostly-upward drift with a little sideways wander, like real
            // smoke — not a radial burst from the orb like the other kinds.
            auto driftAngle = -juce::MathConstants<float>::halfPi + (rng.nextFloat() * 2.0f - 1.0f) * 0.55f;
            juce::Point<float> dir (std::cos (driftAngle), std::sin (driftAngle));

            p.pos = orbCentre + juce::Point<float> ((rng.nextFloat() * 2.0f - 1.0f) * orbRadius * 0.6f, 0.0f);
            p.velocity = dir * (16.0f + rng.nextFloat() * 20.0f);
            p.age = 0.0f;
            p.life = 3.2f + rng.nextFloat() * 2.6f;
            p.size = 9.0f + rng.nextFloat() * 13.0f;
            p.hueOffset = 0.0f;
            p.baseAlpha = 0.16f + rng.nextFloat() * 0.08f;
            p.rotation = 0.0f;
            p.rotationSpin = 0.0f;
            p.kind = Particle::Kind::Smoke;
            p.active = true;
            return;
        }
    }

    void timerCallback() override
    {
        auto dt = frameSeconds;
        auto liveliness = livelinessSource ? juce::jlimit (0.0f, 1.0f, livelinessSource()) : 0.0f;

        // Ambient trickle scales with how energetic the orb currently looks;
        // near silence this simply stops emitting rather than idling.
        auto ambientRate = (insane ? 9.0f : 2.4f) * (eco ? 0.5f : 1.0f);
        ambientAccumulator += liveliness * ambientRate * dt;
        while (ambientAccumulator >= 1.0f)
        {
            spawnParticle (false);
            ambientAccumulator -= 1.0f;
        }

        // Smoke wafts continuously regardless of loudness — it's decoration
        // for the theme, not an audio-reactive element.
        if (Theme::kushKomaActive)
        {
            smokeAccumulator += 1.1f * dt;
            while (smokeAccumulator >= 1.0f)
            {
                spawnSmoke();
                smokeAccumulator -= 1.0f;
            }
        }

        auto cullBounds = getLocalBounds().toFloat().expanded (30.0f);
        bool anyActive = false;

        for (auto& p : particles)
        {
            if (! p.active) continue;

            p.pos += p.velocity * dt;
            p.velocity *= 0.99f;
            p.rotation += p.rotationSpin * dt;
            p.age += dt;

            if (p.age >= p.life || ! cullBounds.contains (p.pos))
                p.active = false;
            else
                anyActive = true;
        }

        if (anyActive || ambientAccumulator > 0.0f || smokeAccumulator > 0.0f)
            repaint();
    }

    static constexpr int kMaxParticles = 420; // headroom for Insane Mode's bigger bursts + faster ambient rate
    static constexpr float kLeafTemplateSize = 10.0f;

    std::array<Particle, kMaxParticles> particles;
    juce::Path leafTemplate;
    juce::Point<float> orbCentre;
    float orbRadius = 40.0f;
    float ambientAccumulator = 0.0f;
    float smokeAccumulator = 0.0f;
    bool insane = false;
    bool eco = false;
    float frameSeconds = 1.0f / 45.0f;
    juce::Random rng { 0x9a11e };
    std::function<float()> livelinessSource;
};

} // namespace onyverb::ui
