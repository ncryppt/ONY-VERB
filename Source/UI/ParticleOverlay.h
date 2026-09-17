#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "LeafShape.h"
#include "BeaverShape.h"
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
    tumbling leaf shapes and a steady curling wisp of smoke drifts up from
    the orb regardless of loudness — a themed variant rather than a second
    overlay, since it's the same pool/timer/culling machinery either way.
    The whole window also slowly hazes over the longer that theme stays
    active (see hotboxLevel/paintHotboxHaze), clearing again once you
    switch away from it.

    "Acid Trip" gets its own overlay instead: a hypnotic swirled spiral
    (see spiralLevel/paintSpiralOverlay) that slowly rotates and breathes
    its opacity in and out for as long as the theme is active, fading away
    when you switch to something else.

    "Canada Eh?" swaps the glow dots for small tumbling beavers instead of
    leaves — same pool/timer machinery, just another themed shape. */
class ParticleOverlay final : public juce::Component, private juce::Timer
{
public:
    ParticleOverlay()
    {
        setInterceptsMouseClicks (false, false);
        leafTemplate = makeLeafPath (kLeafTemplateSize);
        beaverTemplate = makeBeaverPath (kBeaverTemplateSize);
        smokeTemplate.addEllipse (-1.0f, -0.65f, 2.0f, 1.3f); // unit streak, stretched along its direction of travel each frame
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
            spawnParticle (true, orbCentre, orbRadius);
    }

    /** A small, quick particle pop at an arbitrary point in this overlay's
        own coordinate space — used for click feedback on any button in the
        plugin, not just the orb's own transient bursts. Uses a much smaller
        spawn radius than the orb since a button click is a point, not an
        area to spread particles across. */
    void spawnBurstAt (juce::Point<float> origin)
    {
        auto count = insane ? 14 : 8;
        if (eco) count /= 2;
        for (int i = 0; i < count; ++i)
            spawnParticle (true, origin, 4.0f);
    }

    /** Starts a light, continuous particle trickle from `source`'s own
        centre position for as long as it's being dragged — the "moving"
        counterpart to spawnBurstAt()'s single click pop. Position is read
        fresh every tick (via `source`) rather than captured once, so it
        follows the control if it's ever moved/resized mid-drag. Safe to
        call again for a `source` already dragging (a no-op) or when the
        emitter pool is full (drag just goes quiet, nothing else breaks). */
    void beginDragTrickle (juce::Component* source)
    {
        for (auto& d : dragSources)
        {
            if (d.component == source) return;
            if (d.component == nullptr) { d = { source, 0.0f }; return; }
        }
    }

    void endDragTrickle (juce::Component* source)
    {
        for (auto& d : dragSources)
            if (d.component == source)
                d = {};
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
                // Oriented along its current (wobbling) velocity so the wisp
                // reads as a curling streak rather than a plain growing dot.
                auto angle = std::atan2 (p.velocity.y, p.velocity.x);
                g.setColour (Theme::textSecondary.withAlpha (alpha));
                g.fillPath (smokeTemplate, juce::AffineTransform::scale (radius).rotated (angle).translated (p.pos));
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

            if (p.kind == Particle::Kind::Beaver)
            {
                g.setColour (Theme::accent.withAlpha (alpha));
                auto scale = (p.size / kBeaverTemplateSize) * (1.0f - t * 0.25f);
                g.fillPath (beaverTemplate, juce::AffineTransform::scale (scale)
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

        if (spiralLevel > 0.0f)
            paintSpiralOverlay (g);

        if (hotboxLevel > 0.0f)
            paintHotboxHaze (g);
    }

private:
    /** A hypnotic swirled-ray spiral, drawn as alternating filled/gap "arms"
        that sweep outward from the centre through a few full turns — the
        same alternating-stripe idea as a classic op-art hypnosis spiral,
        built as vector wedges rather than a raster texture so it stays
        crisp at any window size. Slowly rotates (spiralPhase) and its
        opacity breathes in and out on its own slow cycle (spiralFadePhase);
        spiralLevel (ramped in timerCallback) is the separate fade the whole
        effect does when the Acid Trip theme is switched on/off, so it never
        just pops into view mid-breath. */
    void paintSpiralOverlay (juce::Graphics& g) const
    {
        auto bounds = getLocalBounds().toFloat();
        auto centre = bounds.getCentre();
        auto rMax = centre.getDistanceFrom (bounds.getTopLeft()) + 20.0f;
        constexpr float rMin = 6.0f;
        constexpr int kTotalSlots = 24;
        constexpr float kTotalTurns = 2.75f;
        constexpr int kSteps = 48;

        auto slotAngle = juce::MathConstants<float>::twoPi / (float) kTotalSlots;
        auto halfWidth = slotAngle * 0.42f; // slightly under half a slot, so a thin gap separates each arm

        auto breathing = std::pow (0.5f + 0.5f * std::sin (spiralFadePhase), 1.5f);
        auto alpha = spiralLevel * breathing * 0.22f;
        if (alpha < 0.004f)
            return;

        g.setColour (Theme::accent.withAlpha (alpha));

        for (int slot = 0; slot < kTotalSlots; slot += 2)
        {
            juce::Path arm;
            for (int step = 0; step <= kSteps; ++step)
            {
                auto t = (float) step / (float) kSteps;
                auto r = rMin + (rMax - rMin) * t;
                auto centreAngle = (float) slot * slotAngle + kTotalTurns * juce::MathConstants<float>::twoPi * t + spiralPhase;
                juce::Point<float> p (centre.x + r * std::cos (centreAngle - halfWidth), centre.y + r * std::sin (centreAngle - halfWidth));
                if (step == 0) arm.startNewSubPath (p); else arm.lineTo (p);
            }
            for (int step = kSteps; step >= 0; --step)
            {
                auto t = (float) step / (float) kSteps;
                auto r = rMin + (rMax - rMin) * t;
                auto centreAngle = (float) slot * slotAngle + kTotalTurns * juce::MathConstants<float>::twoPi * t + spiralPhase;
                arm.lineTo (centre.x + r * std::cos (centreAngle + halfWidth), centre.y + r * std::sin (centreAngle + halfWidth));
            }
            arm.closeSubPath();
            g.fillPath (arm);
        }
    }

    /** A slow-building room haze layered on top of everything else while the
        Kush Koma theme is active — a handful of soft, low-alpha gradient
        blobs that drift lazily around the window (via sine/cosine offsets
        driven by hotboxTime, not a fresh random walk) rather than one flat
        tint, so it reads as uneven smoke hanging in the air instead of a
        dirty screen filter. hotboxLevel itself ramps up/down elsewhere. */
    void paintHotboxHaze (juce::Graphics& g) const
    {
        auto bounds = getLocalBounds().toFloat();
        auto diag = bounds.getWidth() + bounds.getHeight();

        g.setColour (Theme::textSecondary.withAlpha (hotboxLevel * 0.05f));
        g.fillRect (bounds);

        juce::Random blobRng { 0x1105 }; // fixed seed: blobs keep the same identity frame to frame, only their phase animates
        for (int i = 0; i < 5; ++i)
        {
            auto seedX = blobRng.nextFloat();
            auto seedY = blobRng.nextFloat();
            auto freq = 0.05f + blobRng.nextFloat() * 0.05f;
            auto phase = blobRng.nextFloat() * juce::MathConstants<float>::twoPi;

            auto cx = bounds.getX() + bounds.getWidth()  * (seedX + 0.15f * std::sin (hotboxTime * freq + phase));
            auto cy = bounds.getY() + bounds.getHeight() * (seedY + 0.15f * std::cos (hotboxTime * freq * 0.8f + phase));
            auto radius = diag * (0.22f + 0.06f * std::sin (hotboxTime * freq * 1.3f + phase));

            juce::ColourGradient grad (Theme::textSecondary.withAlpha (hotboxLevel * 0.10f), cx, cy,
                                        Theme::textSecondary.withAlpha (0.0f), cx + radius, cy, true);
            g.setGradientFill (grad);
            g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre ({ cx, cy }));
        }
    }

    struct Particle
    {
        enum class Kind { Glow, Leaf, Smoke, Beaver };

        juce::Point<float> pos, velocity;
        float age = 0.0f, life = 1.0f, size = 2.0f, hueOffset = 0.0f, baseAlpha = 0.8f;
        float rotation = 0.0f, rotationSpin = 0.0f;
        // Smoke-only: drives the side-to-side curl applied to velocity.x
        // each frame (see timerCallback), so a wisp wanders rather than
        // travelling in a straight line.
        float wobblePhase = 0.0f, wobbleFreq = 1.0f, wobbleAmp = 0.0f;
        Kind kind = Kind::Glow;
        bool active = false;
    };

    /** One knob/slider currently being dragged; nullptr component = unused
        slot. Position is read from the component itself each tick rather
        than stored, so the trickle keeps following it. */
    struct DragSource
    {
        juce::Component* component = nullptr;
        float accumulator = 0.0f;
    };

    void spawnParticle (bool burst, juce::Point<float> origin, float radius)
    {
        for (auto& p : particles)
        {
            if (p.active) continue;

            auto speedMul = insane ? 2.2f : 1.0f;
            auto sizeMul = insane ? 1.8f : 1.0f;

            auto angle = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            juce::Point<float> direction (std::cos (angle), std::sin (angle));
            auto speed = (burst ? (150.0f + rng.nextFloat() * 190.0f) : (25.0f + rng.nextFloat() * 55.0f)) * speedMul;
            auto startRadius = radius * (0.55f + rng.nextFloat() * 0.5f);

            p.pos = origin + direction * startRadius;
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
            else if (Theme::canadaActive)
            {
                p.kind = Particle::Kind::Beaver;
                p.size *= 3.2f; // same idea — a beaver needs to be bigger than a glow dot to read as one
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

            // A narrow, mostly-upward plume that curls from side to side as
            // it rises (via the per-frame wobble applied in timerCallback)
            // rather than travelling in a straight line — closer to how
            // smoke off a lit joint actually drifts than a simple radial
            // puff.
            auto driftAngle = -juce::MathConstants<float>::halfPi + (rng.nextFloat() * 2.0f - 1.0f) * 0.25f;
            juce::Point<float> dir (std::cos (driftAngle), std::sin (driftAngle));

            p.pos = orbCentre + juce::Point<float> ((rng.nextFloat() * 2.0f - 1.0f) * orbRadius * 0.3f, 0.0f);
            p.velocity = dir * (14.0f + rng.nextFloat() * 14.0f);
            p.age = 0.0f;
            p.life = 3.8f + rng.nextFloat() * 3.0f;
            p.size = 4.0f + rng.nextFloat() * 5.0f; // starts as a thin wisp, grows as it rises (see paint())
            p.hueOffset = 0.0f;
            p.baseAlpha = 0.15f + rng.nextFloat() * 0.07f;
            p.rotation = 0.0f;
            p.rotationSpin = 0.0f;
            p.wobblePhase = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            p.wobbleFreq = 0.9f + rng.nextFloat() * 1.4f;
            p.wobbleAmp = 9.0f + rng.nextFloat() * 12.0f;
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
            spawnParticle (false, orbCentre, orbRadius);
            ambientAccumulator -= 1.0f;
        }

        // Knobs/sliders currently being dragged get their own light trickle,
        // independent of the orb's liveliness — this is drag feedback, not
        // an audio-reactive effect.
        for (auto& d : dragSources)
        {
            if (d.component == nullptr) continue;
            d.accumulator += kDragTrickleRate * dt;
            while (d.accumulator >= 1.0f)
            {
                auto origin = getLocalPoint (d.component, d.component->getLocalBounds().getCentre().toFloat());
                spawnParticle (false, origin, 4.0f);
                d.accumulator -= 1.0f;
            }
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

        // The room haze: slowly builds while the theme is active (the "hot
        // box" effect), and clears out noticeably faster once it isn't —
        // like cracking a window rather than the fog just vanishing.
        hotboxLevel = Theme::kushKomaActive
            ? juce::jmin (1.0f, hotboxLevel + dt / 55.0f)
            : juce::jmax (0.0f, hotboxLevel - dt / 8.0f);
        if (hotboxLevel > 0.0f)
            hotboxTime += dt;

        // The Acid Trip spiral fades in/out quickly when the theme itself is
        // switched on/off, then keeps rotating and breathing its own opacity
        // for as long as it stays visible.
        spiralLevel = Theme::acidTripActive
            ? juce::jmin (1.0f, spiralLevel + dt / 3.0f)
            : juce::jmax (0.0f, spiralLevel - dt / 1.5f);
        if (spiralLevel > 0.0f)
        {
            spiralPhase += dt * 0.05f;
            spiralFadePhase += dt * (juce::MathConstants<float>::twoPi / 10.0f);
            if (spiralFadePhase > juce::MathConstants<float>::twoPi)
                spiralFadePhase -= juce::MathConstants<float>::twoPi;
        }

        auto cullBounds = getLocalBounds().toFloat().expanded (30.0f);
        bool anyActive = false;

        for (auto& p : particles)
        {
            if (! p.active) continue;

            if (p.kind == Particle::Kind::Smoke)
                p.velocity.x = std::sin (p.age * p.wobbleFreq + p.wobblePhase) * p.wobbleAmp;

            p.pos += p.velocity * dt;
            p.velocity *= 0.99f;
            p.rotation += p.rotationSpin * dt;
            p.age += dt;

            if (p.age >= p.life || ! cullBounds.contains (p.pos))
                p.active = false;
            else
                anyActive = true;
        }

        if (anyActive || ambientAccumulator > 0.0f || smokeAccumulator > 0.0f || hotboxLevel > 0.0f || spiralLevel > 0.0f)
            repaint();
    }

    static constexpr int kMaxParticles = 420; // headroom for Insane Mode's bigger bursts + faster ambient rate
    static constexpr float kLeafTemplateSize = 10.0f;
    static constexpr float kBeaverTemplateSize = 10.0f;
    static constexpr int kMaxDragSources = 8;
    static constexpr float kDragTrickleRate = 10.0f; // particles/sec per control being dragged

    std::array<Particle, kMaxParticles> particles;
    std::array<DragSource, kMaxDragSources> dragSources;
    juce::Path leafTemplate;
    juce::Path beaverTemplate;
    juce::Path smokeTemplate;
    juce::Point<float> orbCentre;
    float orbRadius = 40.0f;
    float ambientAccumulator = 0.0f;
    float smokeAccumulator = 0.0f;
    float hotboxLevel = 0.0f;
    float hotboxTime = 0.0f;
    float spiralLevel = 0.0f;
    float spiralPhase = 0.0f;
    float spiralFadePhase = 0.0f;
    bool insane = false;
    bool eco = false;
    float frameSeconds = 1.0f / 45.0f;
    juce::Random rng { 0x9a11e };
    std::function<float()> livelinessSource;
};

/** Wires a small particle pop to fire (from `overlay`, at the button's own
    screen position) whenever `button` is clicked, without disturbing
    whatever onClick it already has — Button::onClick is a single
    std::function, so this wraps rather than replaces it. Used to give
    every button in the plugin the same click feedback as the orb's own
    transient bursts. */
inline void wireClickBurst (juce::Button& button, ParticleOverlay& overlay)
{
    auto previousOnClick = button.onClick;
    button.onClick = [&button, &overlay, previousOnClick]
    {
        auto centre = overlay.getLocalPoint (&button, button.getLocalBounds().getCentre().toFloat());
        overlay.spawnBurstAt (centre);
        if (previousOnClick)
            previousOnClick();
    };
}

/** Same idea as wireClickBurst(), but for continuous movement: starts a
    light particle trickle from `slider`'s own position for as long as
    it's being dragged. Wraps whatever onDragStart/onDragEnd the slider
    already has rather than replacing them. */
inline void wireDragTrickle (juce::Slider& slider, ParticleOverlay& overlay)
{
    auto previousDragStart = slider.onDragStart;
    auto previousDragEnd = slider.onDragEnd;

    slider.onDragStart = [&slider, &overlay, previousDragStart]
    {
        overlay.beginDragTrickle (&slider);
        if (previousDragStart)
            previousDragStart();
    };
    slider.onDragEnd = [&slider, &overlay, previousDragEnd]
    {
        overlay.endDragTrickle (&slider);
        if (previousDragEnd)
            previousDragEnd();
    };
}

} // namespace onyverb::ui
