#pragma once

#include "DspDefines.h"
#include "DspMath.h"

#include <array>

namespace ppf42 {

struct FluidEngineParams {
    float viscosity   { 0.25f }; // Fluid Viscosity [0.0, 1.0]
    float moisture    { 0.20f }; // Moisture Level [0.0, 1.0]
    float dropletRate { 0.30f }; // Splatter Density [0.0, 1.0]
};

class FluidNoiseEngine {
public:
    static constexpr size_t kMaxBubbleVoices = 16;
    static constexpr size_t kMaxDropletFilters = 4;

    FluidNoiseEngine() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Process a single sample: takes air velocity and aperture from oscillator
    [[nodiscard]] float processSample(float airVelocity, float aperture, const FluidEngineParams& p) noexcept;

    // Direct trigger for droplet events (e.g., for external impulses or testing)
    void triggerDroplet(float intensity) noexcept;
    void triggerBubble(float radiusMm, float intensity) noexcept;

    // Explosive cluster trigger on mucosal flap opening transient
    void triggerFlapBurst(float openingIntensity, float moisture, float dropletRate) noexcept;

    [[nodiscard]] float getBubbleActivity() const noexcept { return mActiveBubbleActivity; }
    [[nodiscard]] float getDropletPopTrigger() const noexcept { return mLastDropletTrigger; }

private:
    struct BubbleVoice {
        bool  active       { false };
        float phase        { 0.0f };
        float phaseInc     { 0.0f };
        float envelope     { 0.0f };
        float decayRate    { 0.98f };
        float frequencyHz  { 1500.0f };
    };

    float mSampleRate { 48000.0f };
    FastPrng mPrng { 0x504F4F50454E55ULL };

    // Jet turbulence pinking filter state
    float mPinkB0 { 0.0f };
    float mPinkB1 { 0.0f };
    float mPinkB2 { 0.0f };

    // Minnaert bubble voice pool (16 voices, 0 dynamic allocation)
    std::array<BubbleVoice, kMaxBubbleVoices> mBubbleVoices {};
    int mBubbleSpawnCounter { 0 };

    // Multiphase droplet click filters (4-voice overlapping pool)
    int mSamplesUntilNextDroplet { 4000 };
    std::array<BiquadDirectForm2T, kMaxDropletFilters> mDropletFilters {};
    std::array<float, kMaxDropletFilters> mDropletImpulses {};
    size_t mDropletFilterIdx { 0 };

    // Air stream micro-bubble gurgle lowpass filter
    OnePoleLowpass mGurgleFilter;

    // Fluid viscosity acoustic filter
    OnePoleLowpass mViscosityFilter;

    // Telemetry output state
    float mActiveBubbleActivity { 0.0f };
    float mLastDropletTrigger   { 0.0f };
};

} // namespace ppf42
