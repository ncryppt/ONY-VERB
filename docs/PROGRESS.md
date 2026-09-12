# Progress Log

## Phase 1 — Scaffold (done)

- CMake + JUCE 9.0.2 (pinned git submodule at `modules/JUCE`).
- Plugin metadata: manufacturer "ONYVA", product "ONY Verb", bundle ID
  `com.onyva.onyverb`, category Reverb/Ambience.
- Format targets: VST3 + AU (macOS) + Standalone, all from one CMake
  configuration. AAX target gated behind `-DAAX_SDK_PATH=...` since the SDK
  is NDA'd and not vendored.
- Universal macOS binary (`x86_64;arm64`) via `CMAKE_OSX_ARCHITECTURES`.
- Environment note: this machine's `xcode-select` points at a broken
  Xcode.app install (missing `CoreDevice`/`Mercury` framework symbols, so
  `xcodebuild`/`git`/`clang` fail through the default wrapper). Worked around
  by using the standalone Command Line Tools directly
  (`/Library/Developer/CommandLineTools/usr/bin`) and the "Unix Makefiles"
  CMake generator, which never shells out to `xcodebuild`. No system files
  were changed. If a real Xcode build/IDE project is needed later, the
  Xcode.app install itself will need repairing (reinstall or
  `xcode-select -s` to a working Xcode, which needs sudo).

## Phase 2 — FDN core + parameter tree (done)

- `Source/DSP/` is a self-contained DSP layer (`juce_core` /
  `juce_audio_basics` / `juce_dsp` only — no `juce_audio_processors` or
  graphics dependency), so it's directly unit-testable and shared as-is
  between the plugin and the standalone app:
  - `FDNTank` — 8-line Feedback Delay Network, Hadamard feedback matrix
    (orthogonal, energy-preserving, butterfly-cheap), per-line damping +
    low-cut + high-cut filters, per-line modulation (decorrelated
    phase/rate per line to avoid metallic ringing), freeze support, shimmer
    injection point.
  - `EarlyReflections` — independent stereo multi-tap module, mode-tuned
    tap spacing/gain, scaled by Size.
  - `PitchShifterOctaveUp` — two-tap overlap-add delay-based shifter,
    feeds the Shimmer mode's feedback path.
  - `ModeDefinitions.h` — six modes (Room/Hall/Plate/Chamber/Shimmer/
    Ambient) as genuinely different tuning tables (delay-line ratios, ER
    tap patterns, diffusion coefficients, tone tilt, damping bias), not
    parameter presets on one shared algorithm.
  - `FDNReverbEngine` — orchestrates input gain/filtering, pre-delay,
    4-stage input diffusion, early reflections (parallel), the tank,
    stereo reconstruction from tank taps (width-controlled), dry/wet mix,
    output gain. Block-rate one-pole parameter smoothing throughout, plus a
    short mute/recover fade on mode switches, to avoid zipper noise/clicks.
- `Source/Parameters.h` — full `AudioProcessorValueTreeState` layout for
  every parameter in the brief (Mode, Size, Decay, Freeze, Pre-Delay,
  Diffusion, Damping, Low/High Cut, Width, Mod Depth/Rate, Early Reflections
  level, Mix, Input/Output Gain, Bypass).
- `PluginProcessor` wires APVTS straight to engine setters each block;
  `PluginEditor` is currently `juce::GenericAudioProcessorEditor` (a plain,
  functional parameter list) — intentionally a placeholder per the brief's
  phased plan, to be replaced in Phase 4.
- `Tests/DSPTests.cpp` (JUCE `UnitTest`, no extra test framework dependency):
  finite-output check across all 6 modes, freeze stability over 500 blocks,
  no-click check on instant parameter jumps, mode-cycling stability, and
  exact-passthrough bypass check. All passing as of this commit.

Found/fixed one real design bug during this phase: `ModeDefinitions.h`
(DSP layer) was `#include`-ing `Parameters.h`, which pulls in
`juce_audio_processors` (and transitively `juce_graphics`) just to get the
`ReverbMode` enum — silently breaking the "headless, testable" requirement.
Fixed by moving `ReverbMode`/`getModeNames()` into `Source/DSP/ReverbMode.h`
(zero JUCE-processor dependency) and having `Parameters.h` depend on the DSP
layer instead of the reverse.

## Build verification (this machine)

All four targets build clean with the Unix Makefiles generator:
`ONYVerbTests` (all 5 test cases pass), `ONYVerb_Standalone` (launched, ran,
quit with no crash), `ONYVerb_VST3`, `ONYVerb_AU`. The VST3/AU builds used
JUCE's default `COPY_PLUGIN_AFTER_BUILD` behaviour, which installs straight
to `~/Library/Audio/Plug-Ins/VST3/ONY Verb.vst3` and
`~/Library/Audio/Plug-Ins/Components/ONY Verb.component` — i.e. any DAW on
this Mac will now see "ONY Verb" in its plugin list. Harmless for
dev/testing, but flagging it since it's a write outside the project folder.

Two bugs found and fixed while getting a clean build:

1. `PluginProcessor.cpp` had a free function returning
   `juce::AudioProcessor::BusesProperties` — that type is `protected` on
   `AudioProcessor`, so it's only accessible from within a derived class.
   Fixed by making it a private static method of `OnyVerbProcessor`.
2. JUCE 9.0.2's `juce_CoreMidi_mac.mm` (a MIDI 2.0/UMP-endpoint code path we
   don't use — this plugin has no MIDI I/O) uses
   `std::shared_ptr receiver = rawToUniquePtr(...)`, relying on CTAD to infer
   the type. The Clang/libc++ on this machine — extremely new, effectively a
   macOS 26 beta toolchain — finds that deduction ambiguous. Patched the two
   lines in the vendored submodule to specify the template argument
   explicitly (`std::shared_ptr<ConnectionToSrc>` /
   `std::weak_ptr<ConnectionToSrc>`), which is unambiguous regardless of
   libc++ version and doesn't change behaviour. **This is a local edit to
   `modules/JUCE`**, so `git submodule status` will show it as modified
   relative to the pinned `9.0.2` tag — flagging this rather than silently
   committing a patched submodule. It'll stop being needed once either this
   Mac's Xcode/CLT or JUCE ships a fix upstream.

Separately (unrelated to the plugin code): this machine's `xcode-select`
points at a corrupted `/Applications/Xcode.app` (missing `CoreDevice`/
`Mercury` framework symbols), which breaks `git`/`clang`/`codesign`/etc. when
invoked through the default wrapper. Worked around for building by calling
the standalone Command Line Tools directly and using the "Unix Makefiles"
CMake generator (which never shells out to `xcodebuild`). `codesign` isn't
reachable this way, so the VST3/AU/Standalone builds fell back to JUCE's
ad-hoc signing automatically — fine for local testing, but real
signing/notarizing (see README) will need the system Xcode actually fixed
first (reinstall, or `xcode-select -s` to a working install — needs sudo, so
that's a step for you to run, not something done here).

## Phase 4 — Custom UI (done)

Found the real logo assets in `~/Downloads` (`ONYVA_Logo-white.png` /
`-black.png` / `-grey.png`) and copied white+black into `Resources/` —
they're a bold rounded "ONYVA" wordmark in a pill outline with "RECORDS"
below, monochrome (no inherent accent colour). Per the brief's fallback,
picked a single electric-blue accent (`Source/UI/Theme.h`) used consistently
everywhere rather than introducing a second colour.

Built as isolated, reusable `juce::Component`s under `Source/UI/`, wired to
real parameters (Phase 4 and 5 combined — building the components against
mock data and then rewiring them felt like wasted motion given the DSP/param
layer already existed):

- `OnyvaLookAndFeel` — custom-drawn rotary knobs (glow arc + drop shadow),
  vertical gain rails, the horizontal Character/Diffusion slider, pill
  buttons — overrides on `LookAndFeel_V4`, not stock JUCE chrome.
- `OrbVisualizer` — the hero element. Genuinely audio-reactive: pulses off
  `tailEnergy` (RMS of the wet tank signal, pre-mix) and `outputLevel`, with
  a subtle idle breathing animation so it's never static, and a distinct
  slow-rotating ring when Freeze is active.
  `Source/DSP/VisualizationData.h` adds a `VisualizationSnapshot` (level,
  tail energy, stereo correlation, brightness) computed once per block
  inside `FDNReverbEngine::process()`, pushed through a lock-free
  `juce::AbstractFifo`-backed ring buffer the processor owns and the UI
  drains on a 45Hz timer — the "thread-safe ring buffer" the brief asked
  for, not a stand-in.
- `DecayCurveDisplay` — top-strip T60-vs-frequency curve, purely a function
  of Size/Decay/Damping/Low+High Cut (no audio needed for this one, it's
  parameter-driven, unlike the orb), smoothed so parameter changes animate.
- `CorrelationMeter` — small bottom-right L/R correlation meter, also fed by
  the visualization ring buffer.
- `ModePillBar` — six pill buttons bound to the `mode` choice parameter via
  `juce::ParameterAttachment`.
- `HeaderBar` — logo (from `BinaryData`, compiled in via
  `juce_add_binary_data`, not loaded from disk at runtime), title, Bypass
  (bound via `ButtonParameterAttachment`), and working A/B compare (swaps
  the full `AudioProcessorValueTreeState` state between two in-memory
  snapshots).
- `PresetBar` — combo + prev/next arrows, real save/load of user presets as
  XML under `~/Library/Application Support/ONYVA/ONY Verb/Presets` (factory
  preset content itself is Phase 6).
- `KnobWithLabel` / `CharacterSlider` / `VerticalFader` — the repeated
  parameter-control units, each a `juce::Slider` + `SliderParameterAttachment`
  wrapped with name/value labels.

`PluginEditor` assembles all of this, replacing the Phase 1/2
`GenericAudioProcessorEditor` placeholder. Resizable (760×560 to 1600×1200),
default 960×720.

Bugs hit and fixed while getting this to build: `SliderParameterAttachment`
takes a `RangedAudioParameter&`, not the `AudioProcessorValueTreeState&`
directly (fixed all three call sites); JUCE's binary-data generator sanitises
`ONYVA_Logo-white.png` to the symbol `ONYVA_Logowhite_png` (dash dropped, not
turned into an underscore) — used the actual generated name instead of the
naively-expected one.

Verified by building all three formats (`ONYVerb_Standalone`, `ONYVerb_VST3`,
`ONYVerb_AU` — all succeeded) and launching the standalone app: it ran
stably for several seconds under real CPU load (audio + UI timers running)
and quit cleanly with no crash log. Couldn't screenshot it from this
sandbox (no Screen Recording/Accessibility permission here) — worth opening
it yourself to see the actual rendering before calling the visual design
signed off.

## Custom UI iteration (done)

Several rounds of user-driven polish on the Phase 4 editor: fixed the logo
sitting flush against the window edge (added a left inset), reworked
`OrbVisualizer` to use a single continuous radial gradient instead of
stacked flat-alpha circles, added a specular highlight, a slow segmented
"HUD" ring, and real transient-triggered pulse rings (a snare hit visibly
pings the orb, detected from a fast rise in smoothed tail energy). Pulled
Low Cut/Mix/High Cut into a larger "featured" knob row above the rest,
converted Decay from a knob to a horizontal slider under the existing
Character/Diffusion slider, and aligned the two remaining knob rows to a
shared column grid (previously each row divided the full width by its own
item count, so a 4-knob row and a 3-knob row didn't line up).

## Phase 6 — Factory presets, branding, signing (done)

- `Source/Presets/FactoryPresets.h` — 12 factory presets, two per mode, each
  a distinct starting point (e.g. Hall's "Concert Hall" vs "Cathedral" differ
  in size/decay/pre-delay/diffusion, not just a name change). Stored as a
  sparse list of `{paramID, value}` pairs applied directly via
  `param->setValueNotifyingHost(param->convertTo0to1(value))` — compiled into
  the plugin (no file I/O, always available) rather than shipped as loose
  preset files that could go missing.
- `PresetBar` now lists Factory presets first (with a "Factory" section
  heading), then a "User" section for anything saved locally — prev/next
  arrows and the dropdown both work across the combined list. Saving/loading
  user presets as XML was already real (from Phase 4); this just adds the
  factory tier alongside it.
- `scripts/sign_and_notarize_macos.sh` — signs, notarizes, and staples the
  VST3/AU/Standalone bundles in one pass, given a Developer ID identity and a
  notarytool keychain profile (env vars, refuses to run without them rather
  than silently skipping). `scripts/sign_windows.ps1` — signs the Windows
  VST3 with signtool given a .pfx certificate. Neither has been run against
  real credentials (none available here) — the scripts are the deliverable,
  actually signing/notarizing a release is on you.
- Branding: the two logo variants (white/black) found in `~/Downloads` are
  compiled in via `juce_add_binary_data`; no further branding assets were
  requested or needed beyond what Phase 4 already used.

Verified: all three formats (Standalone/VST3/AU) build clean, all 5 DSP unit
tests still pass, standalone app relaunched successfully with the preset
dropdown populated.

## Theme switcher (done)

Added 6 dark themes (Electric Blue, Amber Ember, Emerald Noir, Crimson
Velvet, Violet Dusk, Mono Slate) — each its own near-black background/panel/
text tint plus a single accent colour, keeping the brief's "one elegant
accent" rule per-theme rather than introducing multiple hues within one.
`Source/UI/Theme.h` now holds `inline` (mutable) colour globals plus a
`ThemePalette` list and `applyPalette()`; a `ThemeSwitcher` dropdown sits in
the preset-bar row (left of the preset combo) and calls back into
`PluginEditor::applyTheme()`, which reassigns the globals, re-applies the
LookAndFeel's base colours (`refreshColours()`), and explicitly re-applies
the handful of `Label::setColour(...)` calls that bake a colour in at
construction time rather than reading Theme:: live (`refreshTheme()` on
`HeaderBar`, `KnobWithLabel`, `CharacterSlider`, `VerticalFader`) — the rest
of the UI reads `Theme::` fresh every `paint()` and just needs a `repaint()`.
Selection persists across reloads via a small text file next to the presets
folder.

Hit one real bug getting it to compile: `juce::Colour`'s `uint32`
constructor is `explicit`, and nested aggregate-initializer-list members are
copy-initialized per the standard — so the bare `{ 0xff0a0a0c }` syntax that
works for a plain `juce::Colour x { ... };` doesn't work as a `ThemePalette`
struct member inside an array literal. Fixed with a tiny `C(uint32)` helper
function so each entry is an ordinary (direct-init-friendly) function call.

Verified: all three formats rebuilt clean, all 5 DSP tests still pass,
standalone relaunched with the switcher visible and defaulting to Electric
Blue.

## Next: Phase 3 (mode-tuning listening pass, needs your ears) / Phase 7 (host testing in Ableton/Logic/Pro Tools)
