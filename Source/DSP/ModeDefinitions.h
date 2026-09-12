#pragma once

#include "ReverbMode.h"
#include <array>

namespace onyverb::dsp
{

/** Number of delay lines in the FDN tank. A power of two keeps the
    Hadamard feedback-mixing matrix a simple butterfly (additions/negations
    only, no multiplies), which is what keeps this cheap enough for 32+
    simultaneous instances. */
static constexpr int kNumTankLines = 8;

/** Per-mode tuning: each mode is a genuinely different algorithm topology,
    not just a parameter preset on top of one shared tank. Delay-line ratios
    are mutually-prime-ish so the tank's modes don't line up into audible
    resonances; tap patterns and tone tilts give each mode its own character. */
struct ModeTuning
{
    // Base delay-line lengths in milliseconds at size == 0.5 (scaled by the
    // Size parameter at run time). Ratios chosen to avoid common factors.
    std::array<float, kNumTankLines> tankDelaysMs;

    // Stereo early-reflection tap times (ms) and gains, L and R slightly
    // offset to keep the early field wide without a room-swap illusion.
    std::array<float, 6> erTapTimesMs;
    std::array<float, 6> erTapGains;
    float erStereoOffsetMs;

    // Four-stage input diffusion all-pass coefficients and delay times.
    std::array<float, 4> diffusionDelaysMs;
    std::array<float, 4> diffusionCoeffBase;

    float toneTiltHz;      // pre-input lowpass tilt: brighter (plate) vs warmer (chamber)
    float dampingBias;     // added on top of the user Damping parameter
    float modRateScale;    // per-mode multiplier on the Mod Rate parameter
    float sizeToDelayScale;// how strongly Size scales tank delay lengths
    bool  isShimmer;
    bool  isAmbientWash;   // biases toward very long/infinite character
};

inline const ModeTuning& getModeTuning (ReverbMode mode)
{
    static const ModeTuning room {
        { 13.7f, 17.3f, 21.1f, 26.6f, 31.9f, 37.4f, 43.1f, 49.7f },
        { 4.0f, 8.0f, 13.0f, 19.0f, 26.0f, 34.0f },
        { 0.9f, 0.75f, 0.6f, 0.45f, 0.3f, 0.2f },
        1.3f,
        { 3.1f, 5.7f, 8.3f, 11.9f },
        { 0.6f, 0.6f, 0.6f, 0.6f },
        9000.0f, 0.05f, 0.8f, 0.6f, false, false
    };

    static const ModeTuning hall {
        { 29.3f, 37.1f, 44.8f, 53.9f, 61.7f, 71.3f, 79.9f, 89.1f },
        { 12.0f, 19.0f, 28.0f, 38.0f, 50.0f, 63.0f },
        { 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f },
        2.6f,
        { 5.3f, 9.7f, 14.1f, 19.9f },
        { 0.65f, 0.65f, 0.62f, 0.6f },
        7500.0f, 0.0f, 1.0f, 1.15f, false, false
    };

    static const ModeTuning plate {
        { 9.1f, 11.3f, 14.7f, 17.9f, 21.3f, 24.1f, 27.7f, 31.3f },
        { 2.0f, 3.5f, 5.5f, 8.0f, 11.0f, 15.0f },
        { 0.9f, 0.8f, 0.7f, 0.55f, 0.4f, 0.25f },
        0.6f,
        { 1.7f, 2.9f, 4.3f, 6.1f },
        { 0.7f, 0.7f, 0.68f, 0.68f },
        13000.0f, -0.08f, 0.6f, 0.4f, false, false
    };

    static const ModeTuning chamber {
        { 18.7f, 23.9f, 28.3f, 33.7f, 39.1f, 44.9f, 51.3f, 57.7f },
        { 6.0f, 11.0f, 17.0f, 24.0f, 32.0f, 41.0f },
        { 0.85f, 0.7f, 0.55f, 0.4f, 0.28f, 0.18f },
        1.6f,
        { 4.1f, 7.3f, 10.7f, 14.9f },
        { 0.62f, 0.62f, 0.6f, 0.6f },
        5500.0f, 0.12f, 0.7f, 0.8f, false, false
    };

    static const ModeTuning shimmer {
        { 24.1f, 30.7f, 36.3f, 43.9f, 50.1f, 58.7f, 65.3f, 73.1f },
        { 9.0f, 15.0f, 22.0f, 30.0f, 40.0f, 52.0f },
        { 0.75f, 0.65f, 0.55f, 0.45f, 0.35f, 0.25f },
        2.1f,
        { 4.7f, 8.9f, 12.9f, 18.1f },
        { 0.6f, 0.6f, 0.58f, 0.58f },
        8500.0f, 0.02f, 0.9f, 1.0f, true, false
    };

    static const ModeTuning ambient {
        { 41.3f, 52.9f, 63.7f, 77.1f, 88.3f, 101.9f, 114.7f, 127.3f },
        { 18.0f, 29.0f, 42.0f, 57.0f, 74.0f, 93.0f },
        { 0.7f, 0.62f, 0.55f, 0.45f, 0.35f, 0.28f },
        3.4f,
        { 7.9f, 13.7f, 19.3f, 26.7f },
        { 0.68f, 0.68f, 0.66f, 0.66f },
        6000.0f, -0.05f, 1.3f, 1.4f, false, true
    };

    switch (mode)
    {
        case ReverbMode::room:     return room;
        case ReverbMode::hall:     return hall;
        case ReverbMode::plate:    return plate;
        case ReverbMode::chamber:  return chamber;
        case ReverbMode::shimmer:  return shimmer;
        case ReverbMode::ambient:  return ambient;
        case ReverbMode::numModes: break;
    }
    return hall;
}

} // namespace onyverb::dsp
