#include "SphincterOscillator.h"
#include <cmath>
#include <algorithm>

namespace ppf42 {

void SphincterOscillator::prepare(double sampleRate) noexcept {
    mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
    reset();
}

void SphincterOscillator::reset() noexcept {
    mDisplacement     = 0.001f; // 1 mm initial aperture
    mVelocity         = 0.0f;
    mAirVelocity      = 0.0f;
    mUpstreamPressure = 0.0f;
    mPrevVolumeFlow   = 0.0f;
}

float SphincterOscillator::processSample(const SphincterOscParams& p, float& outAirVelocity, float& outAperture) noexcept {
    if (p.pressure <= 0.0001f) {
        // Zero drive pressure: valve relaxes to resting position, flow ceases
        const float y0 = std::clamp(p.aperture * 0.002f + 0.0005f, 0.0002f, 0.004f);
        mDisplacement = flushDenormal(mDisplacement + 0.05f * (y0 - mDisplacement));
        mVelocity = 0.0f;
        mAirVelocity = 0.0f;
        mUpstreamPressure = 0.0f;
        mPrevVolumeFlow = 0.0f;
        outAirVelocity = 0.0f;
        outAperture = mDisplacement;
        return 0.0f;
    }

    const float dt = 1.0f / mSampleRate;

    // 1. Clamped fundamental frequency and tension offset
    // param_tension provides fine/coarse offset: 0.5 is nominal, [0.0, 1.0] -> [0.85, 1.15]
    const float tensionMultiplier = 0.85f + 0.30f * std::clamp(p.tension, 0.0f, 1.0f);
    const float fTarget = std::clamp(p.frequencyHz * tensionMultiplier, kMinOscFrequencyHz, kMaxOscFrequencyHz);

    // Non-linear relaxation correction: boundary restitution and Bernoulli suction shift limit cycle frequency by ~0.7085
    constexpr float kRelaxationCorrection = 1.4114f;
    const float f0 = std::clamp(fTarget * kRelaxationCorrection, kMinOscFrequencyHz, kMaxOscFrequencyHz * 1.6f);

    // 2. Viscous mass and damping loading
    const float visc = std::clamp(p.viscosity, 0.0f, 1.0f);
    const float mEff = 0.001f * (1.0f + 0.85f * visc); // Effective mass in kg
    const float omega0 = kTwoPi * f0;
    const float k0 = mEff * omega0 * omega0; // Spring constant
    const float r0 = 2.0f * 0.08f * omega0 * mEff * (1.0f + 1.50f * visc); // Damping coefficient

    // 3. Resting aperture y0 (meters)
    const float y0 = std::clamp(p.aperture * 0.002f + 0.0005f, 0.0002f, 0.004f);

    // 4. Upstream colonic pressure head (Pa)
    const float pDrive = p.pressure * 1500.0f; // Up to 1500 Pa (~15 cm H2O)
    
    // Upstream reservoir response tracks acoustic cycle omega0
    const float fillRate = 2.5f * omega0;
    const float currentArea = kSlitWidthW * std::max(0.0f, mDisplacement);
    const float flowVolume = currentArea * mAirVelocity; // m^3 / s
    
    // Upstream pressure derivative
    const float dPup = fillRate * (pDrive - mUpstreamPressure) - (flowVolume * omega0 * 8.0f);
    mUpstreamPressure = std::clamp(mUpstreamPressure + dt * dPup, 0.0f, 3000.0f);
    mUpstreamPressure = flushDenormal(mUpstreamPressure);

    // 5. Airflow velocity through aperture via Bernoulli equation
    if (mDisplacement > 1.0e-6f && mUpstreamPressure > 0.0f) {
        mAirVelocity = std::sqrt(2.0f * mUpstreamPressure / kAirDensityRho);
    } else {
        mAirVelocity = 0.0f;
    }
    mAirVelocity = flushDenormal(mAirVelocity);

    // 6. Flutter / Asymmetric tissue chaos
    const float flutterAmt = std::clamp(p.flutter, 0.0f, 1.0f);
    const float dispOffset = mDisplacement - y0;
    const float kFlutter = k0 * (1.0f + 0.4f * flutterAmt * (dispOffset * dispOffset * 1.0e6f));
    const float rFlutter = r0 * (1.0f - 0.25f * flutterAmt * (mVelocity > 0.0f ? 1.0f : -1.0f));

    // 7. Force balance
    const float springForce = -kFlutter * dispOffset;
    const float dampingForce = -rFlutter * mVelocity;

    // Tissue cross-section area (0.025m x 0.010m = 0.00025 m^2)
    constexpr float tissueArea = 0.00025f;

    // Scale driving force with (f0 / 110)^2 to maintain constant physical excursion across 18 - 350 Hz
    const float freqRatio = f0 / 110.0f;
    const float forceScaling = std::clamp(freqRatio * freqRatio, 0.02f, 15.0f);

    // Bernoulli dynamic suction: when air surges through narrow gap, pressure in gap drops
    const float gapRatio = y0 / std::max(0.0001f, mDisplacement);
    const float dynamicPressure = 0.5f * kAirDensityRho * mAirVelocity * mAirVelocity * gapRatio;
    const float drivingForce = tissueArea * (mUpstreamPressure - dynamicPressure) * forceScaling;

    // Total net force
    const float totalForce = springForce + dampingForce + drivingForce;
    const float acceleration = totalForce / mEff;

    // 8. Semi-implicit Euler integration
    mVelocity += dt * acceleration;
    mVelocity = std::clamp(mVelocity, -20.0f, 20.0f);
    mVelocity = flushDenormal(mVelocity);

    mDisplacement += dt * mVelocity;

    // 9. Boundary contact stiffness and restitution at y = 0
    if (mDisplacement <= 0.0f) {
        mDisplacement = 0.0f;
        constexpr float eRestitution = 0.65f;
        mVelocity = -eRestitution * mVelocity;
        mAirVelocity = 0.0f; // Gap completely closed, air flow halts instantly
    }
    mDisplacement = flushDenormal(mDisplacement);

    outAirVelocity = mAirVelocity;
    outAperture    = mDisplacement;

    // 10. Radiated acoustic signal: volume velocity rate + displacement
    const float currentVolumeFlow = kSlitWidthW * mDisplacement * mAirVelocity;
    const float dVolumeFlowDt = (currentVolumeFlow - mPrevVolumeFlow) * dt * 1000.0f;
    mPrevVolumeFlow = currentVolumeFlow;

    const float acousticWave = (mDisplacement - y0) * 800.0f + dVolumeFlowDt * 0.1f;
    return flushDenormal(acousticWave);
}

} // namespace ppf42
