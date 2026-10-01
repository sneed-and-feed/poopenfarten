#pragma once

#include "DspDefines.h"
#include "DspMath.h"

namespace ppf42 {

struct SphincterOscParams {
    float pressure    { 0.70f };  // Colonic Drive [0.0, 1.0]
    float tension     { 0.50f };  // Tissue Elasticity offset [0.0, 1.0]
    float aperture    { 0.35f };  // Resting Gap y0 [0.0, 1.0]
    float flutter     { 0.15f };  // Chaos / Asymmetry [0.0, 1.0]
    float viscosity   { 0.25f };  // Fluid Viscosity [0.0, 1.0]
    float frequencyHz { 110.0f }; // Fundamental Pitch [18.0, 350.0] Hz
    float moisture    { 0.20f };  // Mucosal Moisture / Stiction [0.0, 1.0]
};

class SphincterOscillator {
public:
    SphincterOscillator() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Process a single sample: returns acoustic volume velocity, outputs instantaneous air velocity and aperture
    [[nodiscard]] float processSample(const SphincterOscParams& p, float& outAirVelocity, float& outAperture) noexcept;

    [[nodiscard]] float getAperture() const noexcept { return mDisplacement; }
    [[nodiscard]] float getAirVelocity() const noexcept { return mAirVelocity; }
    [[nodiscard]] float getUpstreamPressure() const noexcept { return mUpstreamPressure; }
    [[nodiscard]] bool wasOpeningTransient() const noexcept { return mJustOpened; }
    [[nodiscard]] float getOpeningTransientAmp() const noexcept { return mOpeningTransientAmp; }
    [[nodiscard]] float getImpactSlapAmp() const noexcept { return mImpactSlapAmp; }
    [[nodiscard]] bool isStuck() const noexcept { return mStuck; }

private:
    float mSampleRate         { 48000.0f };
    float mDisplacement       { 0.001f };  // y(t) aperture opening (m normalized)
    float mVelocity           { 0.0f };    // dy/dt tissue velocity
    float mAirVelocity        { 0.0f };    // v_air(t)
    float mUpstreamPressure   { 0.0f };    // P_gut(t)
    float mPrevVolumeFlow     { 0.0f };    // For acoustic pressure derivative dU/dt
    FastPrng mPrng            { 0x504F4F50454E55ULL };

    // Mucosal stiction and aperiodic cycle jitter
    bool  mStuck              { false };   // Adhesive mucosal seal state
    float mBurstThreshold     { 350.0f };  // Randomized burst pressure threshold P_burst
    bool  mJustOpened         { false };   // Flag set on sample when seal breaks
    float mOpeningTransientAmp{ 0.0f };    // Violent explosive opening transient amplitude
    float mImpactSlapAmp      { 0.0f };    // Boundary collision impact slap impulse amplitude
};

} // namespace ppf42
