#pragma once

#include "DspDefines.h"
#include "DspMath.h"
#include "SampleBank.h"
#include <algorithm>
#include <cmath>

namespace ppf42 {

// ============================================================================
// SamplePitcherEngine: High-Fidelity Catmull-Rom Hermite Resampling Engine
// Pitches authentic embedded acoustic recordings across MIDI notes with zero allocations.
// ============================================================================
class SamplePitcherEngine {
public:
    SamplePitcherEngine() noexcept = default;

    void prepare(double sampleRate) noexcept {
        mSampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
        reset();
    }

    void reset() noexcept {
        mPlayhead = 0.0;
        mActive = false;
        mFinished = true;
        mCurrentSampleIndex = 0;
        mSampleLength = 0;
        mSampleData = nullptr;
        mReverse = false;
    }

    void noteOn(int sampleIndex, float startOffset01, bool reverse) noexcept {
        const size_t idx = static_cast<size_t>(std::clamp(sampleIndex, 0, static_cast<int>(kNumEmbeddedSamples - 1)));
        mCurrentSampleIndex = idx;

        const SampleInfo& info = SampleBank::getSample(idx);
        mSampleData = info.data;
        mSampleLength = info.length;
        mReverse = reverse;
        mFinished = (mSampleLength == 0 || mSampleData == nullptr);

        if (mFinished) {
            mActive = false;
            mPlayhead = 0.0;
            return;
        }

        mActive = true;
        const double clampedOffset = static_cast<double>(std::clamp(startOffset01, 0.0f, 0.999f));
        const double maxIdx = static_cast<double>(mSampleLength - 1);

        if (!mReverse) {
            mPlayhead = clampedOffset * maxIdx;
        } else {
            mPlayhead = (1.0 - clampedOffset) * maxIdx;
        }
    }

    void noteOff() noexcept {
        // One-shot sample continues decaying naturally with voice envelope
    }

    [[nodiscard]] bool isActive() const noexcept {
        return mActive && !mFinished;
    }

    [[nodiscard]] bool isFinished() const noexcept {
        return mFinished;
    }

    /**
     * Resamples one sample using 4-point 3rd-order Catmull-Rom Hermite interpolation.
     * @param effectiveFreq Target frequency in Hz (tracking portamento and pitch bend)
     * @param outAirVel Extracted instantaneous acoustic airflow velocity
     * @param outAperture Extracted instantaneous orifice aperture displacement
     * @return Raw resampled acoustic waveform
     */
    [[nodiscard]] float processSample(float effectiveFreq, float& outAirVel, float& outAperture) noexcept {
        if (!mActive || mFinished || mSampleData == nullptr || mSampleLength == 0) {
            outAirVel = 0.0f;
            outAperture = 0.0f;
            return 0.0f;
        }

        // Playhead bounds check
        if (!mReverse) {
            if (mPlayhead >= static_cast<double>(mSampleLength - 1)) {
                mFinished = true;
                outAirVel = 0.0f;
                outAperture = 0.0f;
                return 0.0f;
            }
        } else {
            if (mPlayhead <= 0.0) {
                mFinished = true;
                outAirVel = 0.0f;
                outAperture = 0.0f;
                return 0.0f;
            }
        }

        // 4-point indices for Hermite interpolation
        const int64_t i0 = static_cast<int64_t>(std::floor(mPlayhead));
        const float frac = static_cast<float>(mPlayhead - static_cast<double>(i0));

        const int64_t len = static_cast<int64_t>(mSampleLength);
        const int64_t im1 = std::clamp(i0 - 1, int64_t{0}, len - 1);
        const int64_t i_0 = std::clamp(i0,     int64_t{0}, len - 1);
        const int64_t i1  = std::clamp(i0 + 1, int64_t{0}, len - 1);
        const int64_t i2  = std::clamp(i0 + 2, int64_t{0}, len - 1);

        const float ym1 = mSampleData[im1];
        const float y0  = mSampleData[i_0];
        const float y1  = mSampleData[i1];
        const float y2  = mSampleData[i2];

        // 4-point 3rd-order Catmull-Rom Hermite polynomial
        const float wave = interpolateHermite4P3O(ym1, y0, y1, y2, frac);

        // Derive instantaneous physical aeroacoustic flow properties from authentic sample
        const float absWave = std::abs(wave);
        outAirVel   = absWave * 32.0f;
        outAperture = 0.001f + 0.0035f * absWave;

        // Pitch rate step relative to root MIDI 48 (C3 = 130.8128 Hz)
        const float safeFreq = std::clamp(effectiveFreq, kMinOscFrequencyHz, kMaxOscFrequencyHz);
        const double pitchRatio = static_cast<double>(safeFreq) / kSampleBaseFreqHz;
        const double rateStep = pitchRatio * (48000.0 / mSampleRate);

        // Advance playhead
        if (!mReverse) {
            mPlayhead += rateStep;
            if (mPlayhead >= static_cast<double>(len - 1)) {
                mFinished = true;
            }
        } else {
            mPlayhead -= rateStep;
            if (mPlayhead <= 0.0) {
                mFinished = true;
            }
        }

        return flushDenormal(wave);
    }

private:
    double       mSampleRate         { 48000.0 };
    double       mPlayhead           { 0.0 };
    bool         mActive             { false };
    bool         mFinished           { true };
    size_t       mCurrentSampleIndex { 0 };
    size_t       mSampleLength       { 0 };
    const float* mSampleData         { nullptr };
    bool         mReverse            { false };
};

} // namespace ppf42
