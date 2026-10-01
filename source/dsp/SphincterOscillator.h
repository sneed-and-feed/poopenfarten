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

private:
    float mSampleRate       { 48000.0f };
    float mDisplacement     { 0.35f }; // y(t) aperture opening (m normalized)
    float mVelocity         { 0.0f };  // dy/dt tissue velocity
    float mAirVelocity      { 0.0f };  // v_air(t)
    float mUpstreamPressure { 0.0f };  // P_gut(t)
    float mPrevVolumeFlow   { 0.0f };  // For acoustic pressure derivative dU/dt
};

} // namespace ppf42
