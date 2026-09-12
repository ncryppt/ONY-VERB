#pragma once

#include <juce_graphics/juce_graphics.h>
#include <array>

namespace onyverb::ui::Theme
{
// All near-black grounds with one accent colour each — per the brief, a
// single elegant accent rather than diluting across multiple hues within
// a theme, but the *choice* of accent (and a matching background/text
// tint) is themeable. These are `inline` (not `const`) so ThemePalette
// switching can overwrite them in place; every component reads them fresh
// each paint() rather than caching, so a switch just needs a repaint —
// except the handful of places that call `Component::setColour(...)` once
// at construction (labels, the LookAndFeel's base colours), which expose
// a `refreshTheme()`/`refreshColours()` method the switcher calls explicitly.
inline juce::Colour background       { 0xff0a0a0c };
inline juce::Colour panel            { 0xff121214 };
inline juce::Colour panelRaised      { 0xff17171a };
inline juce::Colour hairline         { 0xff26262b };
inline juce::Colour textPrimary      { 0xfff2f2f4 };
inline juce::Colour textSecondary    { 0xff8a8a90 };
inline juce::Colour textDim          { 0xff55555a };

inline juce::Colour accent           { 0xff35d4ff };
inline juce::Colour accentDim        { 0xff1c7a94 };
inline juce::Colour accentGlow       { 0x8035d4ff };
inline juce::Colour warnAmber        { 0xffffb454 }; // freeze / destructive-adjacent states only

// Set by applyPalette() below; lets theme-aware extras (leaf iconography,
// smoke, leaf-shaped particles) know when the novelty "Kush Koma" theme is
// active without every component re-deriving it from the palette name.
inline bool kushKomaActive = false;

// Set by applyPalette() below; when true, PluginEditor continuously
// rewrites accent/accentDim/accentGlow itself (a rainbow hue-cycle) rather
// than this being a single static palette, and the orb/particles add extra
// psychedelic flourishes on top.
inline bool acidTripActive = false;

inline juce::Colour withAlpha (juce::Colour c, float a) { return c.withAlpha (a); }

inline juce::Font titleFont (float size)  { return juce::Font (juce::FontOptions (size, juce::Font::bold)); }
inline juce::Font labelFont (float size)  { return juce::Font (juce::FontOptions (size, juce::Font::plain)); }

constexpr float cornerRadius = 10.0f;

// ---------------------------------------------------------------------------
// Palettes
// ---------------------------------------------------------------------------

struct ThemePalette
{
    const char* name;
    juce::Colour background, panel, panelRaised, hairline;
    juce::Colour textPrimary, textSecondary, textDim;
    juce::Colour accent, accentDim, accentGlow, warnAmber;
};

// juce::Colour's uint32 constructor is explicit, which nested aggregate
// init (copy-initializing each member) can't use via bare `{ 0xff... }` —
// this little wrapper makes every entry below a normal function call
// instead, which direct-initializes fine.
inline juce::Colour C (juce::uint32 argb) { return juce::Colour (argb); }

inline const std::array<ThemePalette, 8>& getThemePalettes()
{
    static const std::array<ThemePalette, 8> palettes { {
        { "Electric Blue",
          C (0xff0a0a0c), C (0xff121214), C (0xff17171a), C (0xff26262b),
          C (0xfff2f2f4), C (0xff8a8a90), C (0xff55555a),
          C (0xff35d4ff), C (0xff1c7a94), C (0x8035d4ff), C (0xffffb454) },

        { "Amber Ember",
          C (0xff0c0a09), C (0xff15100d), C (0xff1c1512), C (0xff2e2620),
          C (0xfff5f0ea), C (0xff96897b), C (0xff5c534a),
          C (0xffff9d42), C (0xff945a26), C (0x80ff9d42), C (0xffffe08a) },

        { "Emerald Noir",
          C (0xff090c0a), C (0xff10150f), C (0xff171e16), C (0xff283128),
          C (0xfff0f5f1), C (0xff89968b), C (0xff535e54),
          C (0xff2fe6a0), C (0xff1a8a5f), C (0x802fe6a0), C (0xffffd166) },

        { "Crimson Velvet",
          C (0xff0c0909), C (0xff160f0f), C (0xff1d1414), C (0xff342222),
          C (0xfff5efef), C (0xff988686), C (0xff5e4f4f),
          C (0xffff4d6d), C (0xff992e42), C (0x80ff4d6d), C (0xffffb454) },

        { "Violet Dusk",
          C (0xff0a0910), C (0xff141220), C (0xff1b1829), C (0xff2e2946),
          C (0xfff2f0f8), C (0xff938bab), C (0xff5a5470),
          C (0xffb980ff), C (0xff6b4a96), C (0x80b980ff), C (0xffffb454) },

        { "Mono Slate",
          C (0xff0a0a0b), C (0xff141415), C (0xff1a1a1c), C (0xff2c2c2f),
          C (0xfff4f4f5), C (0xff929296), C (0xff5b5b5f),
          C (0xffe8e8ec), C (0xff87878d), C (0x80e8e8ec), C (0xffffb454) },

        { "Kush Koma",
          C (0xff0a0c08), C (0xff10160c), C (0xff161f10), C (0xff283420),
          C (0xfff1f6ea), C (0xff9bab8c), C (0xff5f6f52),
          C (0xff7ed321), C (0xff4a7d14), C (0x807ed321), C (0xffffd166) },

        { "Acid Trip",
          C (0xff0a0710), C (0xff130b1c), C (0xff1a1026), C (0xff33184a),
          C (0xfff8f0ff), C (0xffc9a8e8), C (0xff7a5a94),
          C (0xffff2fd6), C (0xff9c1c94), C (0x80ff2fd6), C (0xffffe93f) },
    } };
    return palettes;
}

inline void applyPalette (const ThemePalette& p)
{
    background = p.background; panel = p.panel; panelRaised = p.panelRaised; hairline = p.hairline;
    textPrimary = p.textPrimary; textSecondary = p.textSecondary; textDim = p.textDim;
    accent = p.accent; accentDim = p.accentDim; accentGlow = p.accentGlow; warnAmber = p.warnAmber;
    kushKomaActive = juce::String (p.name) == "Kush Koma";
    acidTripActive = juce::String (p.name) == "Acid Trip";
}

// ---------------------------------------------------------------------------
// Persistence — remembers the chosen theme across plugin reloads/sessions.
// ---------------------------------------------------------------------------

inline juce::File getThemeChoiceFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("ONYVA").getChildFile ("ONY Verb").getChildFile ("theme.txt");
}

inline int loadSavedThemeIndex()
{
    auto file = getThemeChoiceFile();
    if (! file.existsAsFile())
        return 0;

    auto index = file.loadFileAsString().trim().getIntValue();
    return juce::isPositiveAndBelow (index, (int) getThemePalettes().size()) ? index : 0;
}

inline void saveThemeIndex (int index)
{
    auto file = getThemeChoiceFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText (juce::String (index));
}

} // namespace onyverb::ui::Theme
