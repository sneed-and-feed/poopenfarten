#pragma once

#include "DspDefines.h"
#include "DspMath.h"

#include <cmath>
#include <algorithm>

namespace ppf42 {

// ============================================================================
// BoundedSaturator: C^1 Cubic Hermite Soft-Knee Saturator & Peak Limiter
// Guarantees zero derivative discontinuities and strict <= 0.0 dBFS ceiling.
// ============================================================================
class BoundedSaturator {
public:
    // C^1 cubic Hermite soft-clipping function
    [[nodiscard]] static inline float saturateHermite(float x, float knee = 0.72f, float ceiling = 1.05f) noexcept {
        if (!isFiniteBitwise(x)) [[unlikely]] {
            return 0.0f;
        }

        const float absX = std::abs(x);
        if (absX <= knee) {
            return x; // Linear pass-through
        }

        const float sgn = (x >= 0.0f) ? 1.0f : -1.0f;
        if (absX >= ceiling) {
            return sgn * ceiling;
        }

        // Hermite polynomial smoothly connecting knee (deriv 1) to ceiling (deriv 0)
        const float u = (absX - knee) / (ceiling - knee);
        const float poly = u + u * u - u * u * u;
        return sgn * (knee + (ceiling - knee) * poly);
    }

    // Process drive stage: applies non-linear harmonic saturation
    [[nodiscard]] static inline float processDrive(float in, float drive) noexcept {
        const float d = std::clamp(drive, 0.0f, 1.0f);
        const float driven = in * (1.0f + 3.5f * d);
        return saturateHermite(driven, 0.72f, 1.05f);
    }

    // True-Peak Brickwall Limiter: guarantees output strictly in [-1.0, +1.0] (<= 0.0 dBFS)
    [[nodiscard]] static inline float limitSample(float in, float masterGainLinear = 1.0f) noexcept {
        if (!isFiniteBitwise(in)) [[unlikely]] {
            return 0.0f;
        }

        const float x = flushDenormal(in * masterGainLinear);
        constexpr float kKnee = kLimiterKnee;         // 0.85
        constexpr float kCeiling = kMaxTruePeakCeiling; // 1.00

        const float absX = std::abs(x);
        if (absX <= kKnee) {
            return x;
        }

        const float sgn = (x >= 0.0f) ? 1.0f : -1.0f;
        if (absX >= kCeiling) {
            return sgn * kCeiling;
        }

        const float u = (absX - kKnee) / (kCeiling - kKnee);
        // poly(u) = u(1 + u(1 - u)) with poly(0) = 0, poly(1) = 1, poly'(0) = 1, poly'(1) = 0
        const float poly = u * (1.0f + u * (1.0f - u));
        return std::clamp(sgn * (kKnee + (kCeiling - kKnee) * poly), -1.0f, 1.0f);
    }
};

} // namespace ppf42
