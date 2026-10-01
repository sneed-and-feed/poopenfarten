#include "PorcelainConvolver.h"
#include <cmath>
#include <algorithm>

namespace ppf42 {

void PorcelainConvolver::prepare(double sampleRate) noexcept {
    mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
    mCurrentModel = -1;
    mCurrentSize = -1.0f;
    reset();
}

void PorcelainConvolver::reset() noexcept {
    mHistoryBuffer.fill(0.0f);
    mWriteIdx = 0;
    // Force rebuild of kernel
    rebuildKernel(mCurrentModel >= 0 ? mCurrentModel : 1, mCurrentSize > 0.0f ? mCurrentSize : 1.0f);
}

void PorcelainConvolver::rebuildKernel(int model, float size) noexcept {
    mCurrentModel = std::clamp(model, 0, 3);
    mCurrentSize  = std::clamp(size, 0.5f, 2.0f);

    mKernel.fill(0.0f);
    const float s = mCurrentSize;
    const float dt = 1.0f / mSampleRate;

    switch (mCurrentModel) {
        case 0: { // Dry Chamber: Direct response with minimal early reflections
            mKernel[0] = 0.90f;
            const size_t d1 = std::clamp(static_cast<size_t>(0.0015f * mSampleRate * s), size_t{1}, kKernelSize - 1);
            const size_t d2 = std::clamp(static_cast<size_t>(0.0030f * mSampleRate * s), size_t{1}, kKernelSize - 1);
            mKernel[d1] = -0.15f;
            mKernel[d2] =  0.08f;
            break;
        }

        case 1: { // Ceramic Bowl - Standard: Resonant porcelain modes at 420 Hz & 1180 Hz
            mKernel[0] = 0.70f;
            const float f1 = (420.0f / s) * kTwoPi;
            const float f2 = (1180.0f / s) * kTwoPi;
            const float f3 = (2450.0f / s) * kTwoPi;
            const float decayTime = 0.045f * s; // Effective resonance duration

            for (size_t i = 1; i < kKernelSize; ++i) {
                const float t = static_cast<float>(i) * dt;
                const float env = std::exp(-t / decayTime);
                const float mode1 = std::sin(f1 * t) * 0.45f;
                const float mode2 = std::sin(f2 * t) * 0.35f;
                const float mode3 = std::sin(f3 * t) * 0.15f;
                mKernel[i] = (mode1 + mode2 + mode3) * env;
            }
            break;
        }

        case 2: { // Ceramic Bowl - Water Coupled: Acoustic low-pass loading, damping above 600 Hz
            mKernel[0] = 0.80f;
            const float f1 = (290.0f / s) * kTwoPi;
            const float f2 = (560.0f / s) * kTwoPi;
            const float decayTime = 0.030f * s;

            for (size_t i = 1; i < kKernelSize; ++i) {
                const float t = static_cast<float>(i) * dt;
                const float env = std::exp(-t / decayTime);
                const float mode1 = std::sin(f1 * t) * 0.60f;
                const float mode2 = std::sin(f2 * t) * 0.25f;
                mKernel[i] = (mode1 + mode2) * env;
            }
            break;
        }

        case 3: { // Tiled Enclosure: Hard bathroom boundary flutter echoes
            mKernel[0] = 0.60f;
            const float flutterInterval = 0.0035f * s;
            const float decayTime = 0.060f * s;

            for (size_t i = 1; i < kKernelSize; ++i) {
                const float t = static_cast<float>(i) * dt;
                const float env = std::exp(-t / decayTime);
                // Dense flutter echo periodicity
                const float flutterPhase = std::fmod(t, flutterInterval) / flutterInterval;
                const float flutterComb = (flutterPhase < 0.15f ? 0.35f : -0.05f);
                const float wallMode = std::sin((850.0f / s) * kTwoPi * t) * 0.25f;
                mKernel[i] = (flutterComb + wallMode) * env;
            }
            break;
        }
    }

    // Peak normalization: ensure direct impulse remains prominent and output never clips
    float maxAbs = 0.0f;
    for (size_t i = 0; i < kKernelSize; ++i) {
        maxAbs = std::max(maxAbs, std::abs(mKernel[i]));
    }
    if (maxAbs > 1.0e-5f) {
        const float norm = 0.85f / maxAbs;
        for (size_t i = 0; i < kKernelSize; ++i) {
            mKernel[i] = flushDenormal(mKernel[i] * norm);
        }
    }
}

float PorcelainConvolver::processSample(float input, float mix, float size, int model) noexcept {
    const float safeMix = std::clamp(mix, 0.0f, 1.0f);
    if (safeMix <= 0.0001f) {
        // Bypass convolution entirely when mix is zero
        return input;
    }

    // Check if model or size changed
    const int targetModel = std::clamp(model, 0, 3);
    const float targetSize = std::clamp(size, 0.5f, 2.0f);
    if (targetModel != mCurrentModel || std::abs(targetSize - mCurrentSize) > 0.01f) {
        rebuildKernel(targetModel, targetSize);
    }

    // Store incoming sample in circular history buffer
    mHistoryBuffer[mWriteIdx] = flushDenormal(input);

    // Direct FIR convolution: zero algorithmic latency
    float convSum = 0.0f;
    size_t histIdx = mWriteIdx;

    for (size_t k = 0; k < kKernelSize; ++k) {
        convSum += mKernel[k] * mHistoryBuffer[histIdx];
        histIdx = (histIdx == 0) ? kKernelMask : (histIdx - 1);
    }

    mWriteIdx = (mWriteIdx + 1) & kKernelMask;

    convSum = flushDenormal(convSum);

    // Wet/dry blend
    return flushDenormal((1.0f - safeMix) * input + safeMix * convSum);
}

} // namespace ppf42
