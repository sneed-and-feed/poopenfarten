#include "FluidNoiseEngine.h"
#include <cmath>
#include <algorithm>

namespace ppf42 {

void FluidNoiseEngine::prepare(double sampleRate) noexcept {
    mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
    mPrng.setSeed(0x504F4F50454E55ULL);
    reset();
}

void FluidNoiseEngine::reset() noexcept {
    mPinkB0 = 0.0f;
    mPinkB1 = 0.0f;
    mPinkB2 = 0.0f;

    for (auto& voice : mBubbleVoices) {
        voice.active = false;
        voice.phase = 0.0f;
        voice.envelope = 0.0f;
    }
    mBubbleSpawnCounter = 200;

    mSamplesUntilNextDroplet = static_cast<int>(mSampleRate * 0.05f);
    mDropletFilter.reset();
    mDropletImpulse = 0.0f;

    mViscosityFilter.reset();
    mActiveBubbleActivity = 0.0f;
    mLastDropletTrigger = 0.0f;
}

void FluidNoiseEngine::triggerBubble(float radiusMm, float intensity) noexcept {
    // Minnaert formula: f = 1 / (2*pi*R) * sqrt(3*gamma*P0 / rho)
    // For gamma=1.4, P0=101325, rho=1000: sqrt(...) = 20.6292
    // f = 20.6292 / (2*pi*R) = 3.28325 / R
    const float rClamped = std::clamp(radiusMm, 0.5f, 4.0f) * 0.001f; // in meters
    const float freq = std::clamp(3.28325f / rClamped, 800.0f, 6500.0f);
    const float nyquist = mSampleRate * 0.49f;
    const float safeFreq = std::min(freq, nyquist);

    // Find best voice: idle voice first, or voice with lowest envelope
    size_t bestIdx = 0;
    float lowestEnv = 1.0e10f;
    for (size_t i = 0; i < kMaxBubbleVoices; ++i) {
        if (!mBubbleVoices[i].active) {
            bestIdx = i;
            break;
        }
        if (mBubbleVoices[i].envelope < lowestEnv) {
            lowestEnv = mBubbleVoices[i].envelope;
            bestIdx = i;
        }
    }

    auto& v = mBubbleVoices[bestIdx];
    v.active = true;
    v.frequencyHz = safeFreq;
    v.phase = mPrng.nextFloat01() * kTwoPi;
    v.phaseInc = safeFreq * (kTwoPi / mSampleRate);
    v.envelope = std::clamp(intensity, 0.01f, 1.0f);

    // Damping ratio zeta ~ 0.10: burst duration ~ 15-30 ms
    const float decayTimeSec = 0.012f + 0.015f * (rClamped * 1000.0f / 4.0f);
    v.decayRate = std::exp(-1.0f / (decayTimeSec * mSampleRate));
}

void FluidNoiseEngine::triggerDroplet(float intensity) noexcept {
    // Randomize droplet bandpass resonance between 1.2 kHz and 4.8 kHz
    const float popFreq = 1200.0f + mPrng.nextFloat01() * 3600.0f;
    const float popQ = 6.0f + mPrng.nextFloat01() * 12.0f; // sharp resonant pop
    mDropletFilter.setBandpass(mSampleRate, popFreq, popQ);
    mDropletImpulse = intensity;
    mLastDropletTrigger = intensity;
}

float FluidNoiseEngine::processSample(float airVelocity, float aperture, const FluidEngineParams& p) noexcept {
    mLastDropletTrigger = 0.0f;

    // 1. Reynolds Jet Turbulence Noise
    // Modulated by |v_air|^2.5 and aperture area. If aperture <= 0 or velocity <= 0, noise is strictly 0.
    float turbSignal = 0.0f;
    const float positiveAirVel = std::max(0.0f, airVelocity);
    const float positiveAperture = std::max(0.0f, aperture);

    if (positiveAperture > 1.0e-6f && positiveAirVel > 0.01f) {
        const float white = mPrng.nextFloatSigned();
        // 3-pole pinking filter (Paul Kellet formulation)
        mPinkB0 = 0.99765f * mPinkB0 + white * 0.0990460f;
        mPinkB1 = 0.96300f * mPinkB1 + white * 0.2965164f;
        mPinkB2 = 0.57000f * mPinkB2 + white * 1.0526913f;
        const float pink = mPinkB0 + mPinkB1 + mPinkB2 + white * 0.1848f;

        const float reynoldsScaling = std::pow(positiveAirVel / 20.0f, 2.5f);
        const float effectiveArea = (kSlitWidthW * positiveAperture) * 100.0f;
        turbSignal = pink * reynoldsScaling * effectiveArea * 0.12f;
        turbSignal = flushDenormal(turbSignal);
    }

    // 2. Multiphase Fluid Engine (Minnaert Bubbles & Poisson Droplets)
    float fluidSignal = 0.0f;
    const float moisture = std::clamp(p.moisture, 0.0f, 1.0f);

    if (moisture > 0.0001f && positiveAirVel > 0.1f) {
        // --- Minnaert Bubble Spawning ---
        if (--mBubbleSpawnCounter <= 0) {
            // Spawn rate proportional to moisture and air velocity
            const float spawnInterval = (0.005f + (1.0f - moisture) * 0.040f) * mSampleRate;
            mBubbleSpawnCounter = std::max(16, static_cast<int>(spawnInterval * (0.8f + 0.4f * mPrng.nextFloat01())));

            const float rMm = 0.5f + mPrng.nextFloat01() * 3.5f; // 0.5 mm to 4.0 mm
            const float intensity = moisture * (0.2f + 0.8f * mPrng.nextFloat01()) * (positiveAirVel / 25.0f);
            triggerBubble(rMm, intensity);
        }

        // --- Poisson Droplet Splatter ---
        if (--mSamplesUntilNextDroplet <= 0) {
            // Exact inverse-CDF interval: Delta_t = -ln(1 - U) / lambda * fs
            const float lambda = std::max(0.1f, (1.0f + p.dropletRate * 39.0f) * moisture * (positiveAirVel / 15.0f));
            const float u = std::clamp(mPrng.nextFloat01(), 1.0e-5f, 0.99999f);
            const float intervalSec = -std::log(1.0f - u) / lambda;
            mSamplesUntilNextDroplet = std::max(12, static_cast<int>(intervalSec * mSampleRate));

            const float dropIntensity = moisture * (0.3f + 0.7f * mPrng.nextFloat01());
            triggerDroplet(dropIntensity);
        }
    }

    // Render active bubble voices
    float bubbleSum = 0.0f;
    float activeEnvs = 0.0f;
    for (auto& voice : mBubbleVoices) {
        if (!voice.active) continue;

        voice.phase += voice.phaseInc;
        if (voice.phase >= kTwoPi) voice.phase -= kTwoPi;

        const float val = std::sin(voice.phase) * voice.envelope;
        bubbleSum += val;
        activeEnvs += voice.envelope;

        voice.envelope *= voice.decayRate;
        if (voice.envelope < 1.0e-4f) {
            voice.active = false;
            voice.envelope = 0.0f;
        }
    }
    mActiveBubbleActivity = std::clamp(activeEnvs * 0.2f, 0.0f, 1.0f);

    // Render droplet pop filter
    float dropSample = 0.0f;
    if (std::abs(mDropletImpulse) > 1.0e-5f || true) {
        dropSample = mDropletFilter.process(mDropletImpulse);
        mDropletImpulse = 0.0f; // Impulse input is single-sample Dirac
    }

    fluidSignal = (bubbleSum * 0.35f + dropSample * 0.45f) * moisture;

    // 3. Sum and Viscosity Filtering
    const float rawOutput = turbSignal + fluidSignal;

    // Viscosity dampens high frequencies: cutoff slides from 14 kHz down to 2.5 kHz
    const float visc = std::clamp(p.viscosity, 0.0f, 1.0f);
    const float viscCutoff = 14000.0f - visc * 11500.0f;
    mViscosityFilter.setCutoff(mSampleRate, viscCutoff);

    return mViscosityFilter.process(rawOutput);
}

} // namespace ppf42
