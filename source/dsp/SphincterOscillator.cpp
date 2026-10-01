#include "SphincterOscillator.h"
#include <cmath>
#include <algorithm>

namespace ppf42 {

void SphincterOscillator::prepare(double sampleRate) noexcept {
    mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
    mPrng.setSeed(0x504F4F50454E55ULL);
    reset();
}

void SphincterOscillator::reset() noexcept {
    mDisplacement        = 0.001f; // 1 mm initial aperture
    mVelocity            = 0.0f;
    mAirVelocity         = 0.0f;
    mUpstreamPressure    = 0.0f;
    mPrevVolumeFlow      = 0.0f;
    mStuck               = false;
    mBurstThreshold      = 350.0f;
    mJustOpened          = false;
    mOpeningTransientAmp = 0.0f;
    mImpactSlapAmp       = 0.0f;
}

float SphincterOscillator::processSample(const SphincterOscParams& p, float& outAirVelocity, float& outAperture) noexcept {
    mJustOpened = false;
    mOpeningTransientAmp = 0.0f;

    if (p.pressure <= 0.0001f) {
        // Zero drive pressure: valve relaxes to resting position, flow ceases
        const float y0 = std::clamp(p.aperture * 0.002f + 0.0005f, 0.0002f, 0.004f);
        mDisplacement = flushDenormal(mDisplacement + 0.05f * (y0 - mDisplacement));
        mVelocity = 0.0f;
        mAirVelocity = 0.0f;
        mUpstreamPressure = 0.0f;
        mPrevVolumeFlow = 0.0f;
        mStuck = false;
        mImpactSlapAmp = 0.0f;
        outAirVelocity = 0.0f;
        outAperture = mDisplacement;
        return 0.0f;
    }

    const float dt = 1.0f / mSampleRate;

    // 1. Clamped fundamental frequency and tension offset
    const float tensionMultiplier = 0.85f + 0.30f * std::clamp(p.tension, 0.0f, 1.0f);
    const float fTarget = std::clamp(p.frequencyHz * tensionMultiplier, kMinOscFrequencyHz, kMaxOscFrequencyHz);

    // Non-linear relaxation correction: boundary restitution, stiction dwell, and Bernoulli suction
    constexpr float kRelaxationCorrection = 1.28f;
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
    const float fillRate = 3.5f * omega0;
    const float currentArea = mStuck ? 0.0f : (kSlitWidthW * std::max(0.0f, mDisplacement));
    const float flowVolume = currentArea * mAirVelocity; // m^3 / s
    
    // Upstream pressure derivative
    const float dPup = fillRate * (pDrive - mUpstreamPressure) - (flowVolume * omega0 * 8.0f);
    mUpstreamPressure = std::clamp(mUpstreamPressure + dt * dPup, 0.0f, 3000.0f);
    mUpstreamPressure = flushDenormal(mUpstreamPressure);

    // 5. Mucosal Stiction & Seal Burst Logic
    // When closed (y <= 0), moisture and surface tension create adhesive seal stiction holding tissue shut
    if (mStuck) {
        // Upstream pressure accumulates behind closed seal until exceeding randomized burst threshold:
        // P_burst = P_threshold * (1.0 + random_jitter_between(-0.35, +0.35))
        if (mUpstreamPressure >= mBurstThreshold || mUpstreamPressure >= (0.96f * pDrive)) {
            // The seal breaks with a violent explosive opening transient!
            mStuck = false;
            mJustOpened = true;

            // Initial explosive opening velocity spike
            const float burstVel = std::sqrt(std::max(0.0f, 2.0f * mUpstreamPressure / mEff)) * 0.035f + 1.8f;
            mVelocity = burstVel;
            mOpeningTransientAmp = burstVel;
            mDisplacement = 0.00015f; // Crack seal immediately
        } else {
            // Valve remains clamped shut by adhesive mucosal stiction
            mDisplacement = 0.0f;
            mVelocity = 0.0f;
            mAirVelocity = 0.0f;
        }
    }

    // 6. Airflow velocity through aperture via Bernoulli equation
    if (!mStuck && mDisplacement > 1.0e-6f && mUpstreamPressure > 0.0f) {
        mAirVelocity = std::sqrt(2.0f * mUpstreamPressure / kAirDensityRho);
    } else {
        mAirVelocity = 0.0f;
    }
    mAirVelocity = flushDenormal(mAirVelocity);

    // 7. Flutter / Asymmetric tissue chaos
    if (!mStuck) {
        const float flutterAmt = std::clamp(p.flutter, 0.0f, 1.0f);
        const float dispOffset = mDisplacement - y0;
        const float kFlutter = k0 * (1.0f + 0.4f * flutterAmt * (dispOffset * dispOffset * 1.0e6f));
        const float rFlutter = r0 * (1.0f - 0.25f * flutterAmt * (mVelocity > 0.0f ? 1.0f : -1.0f));

        // Force balance
        const float springForce = -kFlutter * dispOffset;
        const float dampingForce = -rFlutter * mVelocity;

        constexpr float tissueArea = 0.00025f;
        const float freqRatio = f0 / 110.0f;
        const float forceScaling = std::clamp(freqRatio * freqRatio, 0.02f, 15.0f);

        // Bernoulli dynamic suction: when air surges through narrow gap, pressure in gap drops
        const float gapRatio = y0 / std::max(0.0001f, mDisplacement);
        const float dynamicPressure = 0.5f * kAirDensityRho * mAirVelocity * mAirVelocity * gapRatio;
        const float drivingForce = tissueArea * (mUpstreamPressure - dynamicPressure) * forceScaling;

        // Total net force
        const float totalForce = springForce + dampingForce + drivingForce;
        const float acceleration = totalForce / mEff;

        // Semi-implicit Euler integration
        mVelocity += dt * acceleration;
        mVelocity = std::clamp(mVelocity, -25.0f, 25.0f);
        mVelocity = flushDenormal(mVelocity);

        mDisplacement += dt * mVelocity;

        // 8. Boundary contact stiffness and mucosal stiction capture at y = 0
        if (mDisplacement <= 0.0f) {
            // Walls slam shut with an impact slap (restitution damping) and stick again
            mDisplacement = 0.0f;
            mAirVelocity = 0.0f;
            mStuck = true;

            // Restitution impact slap impulse
            mImpactSlapAmp = std::clamp(-mVelocity * 0.35f, 0.0f, 6.0f);
            mVelocity = 0.0f; // Arrested by mucosal adhesion

            // Calculate randomized burst threshold for next cycle:
            // P_burst = P_threshold * (1.0 + random_jitter_between(-0.35, +0.35))
            const float pThreshold = pDrive * (0.35f + 0.22f * std::clamp(p.tension, 0.0f, 1.0f));
            const float jitter = mPrng.nextFloatSigned() * 0.35f;
            mBurstThreshold = std::max(40.0f, pThreshold * (1.0f + jitter));
        }
    }
    mDisplacement = flushDenormal(mDisplacement);

    outAirVelocity = mAirVelocity;
    outAperture    = mDisplacement;

    // 9. Radiated acoustic signal: volume velocity rate + displacement + opening transient + closing slap
    const float currentVolumeFlow = kSlitWidthW * mDisplacement * mAirVelocity;
    const float dVolumeFlowDt = (currentVolumeFlow - mPrevVolumeFlow) * dt * 1000.0f;
    mPrevVolumeFlow = currentVolumeFlow;

    const float openPulse = mJustOpened ? (mOpeningTransientAmp * 0.12f) : 0.0f;
    const float slapPulse = (mImpactSlapAmp > 0.0f) ? (mImpactSlapAmp * -0.08f) : 0.0f;
    mImpactSlapAmp *= 0.85f;
    if (mImpactSlapAmp < 1.0e-3f) mImpactSlapAmp = 0.0f;

    const float acousticWave = (mDisplacement - y0) * 800.0f + dVolumeFlowDt * 0.10f + openPulse + slapPulse;
    return flushDenormal(acousticWave);
}

} // namespace ppf42
