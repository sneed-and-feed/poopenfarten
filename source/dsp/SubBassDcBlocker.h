#pragma once

#include "DspDefines.h"
#include "DspMath.h"

#include <cmath>
#include <algorithm>

namespace ppf42 {

// ============================================================================
// SubBassDcBlocker: 1-Pole Infrasonic 15 Hz Highpass Filter
// Removes asymmetric gastrointestinal DC offsets while preserving 18 Hz sub-bass
// ============================================================================
class SubBassDcBlocker {
public:
    SubBassDcBlocker() noexcept = default;

    void prepare(double sampleRate) noexcept {
        const float fs = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
        // 15 Hz highpass pole radius: R = 1 - (2*pi*fc / fs)
        const float rRaw = 1.0f - (kTwoPi * 15.0f / fs);
        mPoleRadius = std::clamp(rRaw, 0.0f, 0.99995f);
        reset();
    }

    void reset() noexcept {
        mX1 = 0.0f;
        mY1 = 0.0f;
    }

    [[nodiscard]] inline float processSample(float in) noexcept {
        const float x = flushDenormal(in);
        const float y = x - mX1 + mPoleRadius * mY1;
        mX1 = x;
        mY1 = flushDenormal(y);
        return mY1;
    }

private:
    float mPoleRadius { 0.9980f };
    float mX1         { 0.0f };
    float mY1         { 0.0f };
};

} // namespace ppf42
