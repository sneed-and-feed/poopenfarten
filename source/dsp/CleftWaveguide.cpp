#include "CleftWaveguide.h"
#include <cmath>
#include <algorithm>

namespace ppf42 {

void CleftWaveguide::prepare(double sampleRate) noexcept {
    mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
    reset();
}

void CleftWaveguide::reset() noexcept {
    mDelayLine.fill(0.0f);
    mWriteIdx = 0;
    mDelayedSample = 0.0f;
    mDampingZ1 = 0.0f;
    mResonanceEnergy = 0.0f;
}

float CleftWaveguide::processSample(float input, float damping) noexcept {
    const float safeDamping = std::clamp(damping, 0.0f, 1.0f);

    // Anatomical cleft boundary delay: ~1.2 ms
    const float delaySamples = std::clamp(0.0012f * mSampleRate, 4.0f, static_cast<float>(kMaxDelaySamples - 8));
    const float readPos = static_cast<float>(mWriteIdx) - delaySamples;

    // Fractional delay read index calculation
    const int intPart = static_cast<int>(std::floor(readPos));
    const float frac = readPos - static_cast<float>(intPart);

    const size_t idx0 = static_cast<size_t>(intPart - 1) & kDelayMask;
    const size_t idx1 = static_cast<size_t>(intPart)     & kDelayMask;
    const size_t idx2 = static_cast<size_t>(intPart + 1) & kDelayMask;
    const size_t idx3 = static_cast<size_t>(intPart + 2) & kDelayMask;

    const float ym1 = mDelayLine[idx0];
    const float y0  = mDelayLine[idx1];
    const float y1  = mDelayLine[idx2];
    const float y2  = mDelayLine[idx3];

    const float delayed = interpolateHermite4P3O(ym1, y0, y1, y2, frac);

    // High-frequency boundary absorption filter in feedback path
    // damping: 0.0 = low absorption (brighter reflections), 1.0 = heavy soft tissue damping
    const float dampCutoff = 0.20f + 0.60f * safeDamping;
    mDampingZ1 += (1.0f - dampCutoff) * (delayed - mDampingZ1);
    mDampingZ1 = flushDenormal(mDampingZ1);

    // Feedback comb gain: safe smooth boundary reflection
    const float gCleft = std::clamp(0.35f * (1.0f - 0.50f * safeDamping), 0.0f, 0.45f);
    const float feedback = gCleft * mDampingZ1;

    // Write sum to delay line
    const float toDelay = flushDenormal(input + feedback);
    mDelayLine[mWriteIdx] = toDelay;
    mWriteIdx = (mWriteIdx + 1) & kDelayMask;

    // Direct output
    const float out = flushDenormal(input + feedback * 0.40f);

    // Track smoothed resonance energy for visualizer telemetry
    const float sampleEnergy = out * out;
    mResonanceEnergy = flushDenormal(0.99f * mResonanceEnergy + 0.01f * sampleEnergy);

    return out;
}

} // namespace ppf42
