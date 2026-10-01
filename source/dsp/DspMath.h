#pragma once

#include "DspDefines.h"

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

// Hardware denormal control includes
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#include <xmmintrin.h>
#include <pmmintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#if defined(_MSC_VER)
#include <arm64intr.h>
#endif
#endif

namespace ppf42 {

// ============================================================================
// ScopedNoDenormals: Cross-Platform RAII Hardware FTZ/DAZ Guard
// ============================================================================
class ScopedNoDenormals {
public:
    ScopedNoDenormals() noexcept {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        mOldMxcsr = _mm_getcsr();
        _mm_setcsr(mOldMxcsr | 0x8040); // Bit 15: FTZ (Flush-To-Zero), Bit 6: DAZ (Denormals-Are-Zero)
#elif defined(__aarch64__) || defined(_M_ARM64)
#if defined(_MSC_VER)
        mOldFpcr = _ReadStatusReg(ARM64_FPCR);
        _WriteStatusReg(ARM64_FPCR, mOldFpcr | (1ULL << 24)); // Bit 24: FZ
#elif defined(__GNUC__) || defined(__clang__)
        uint64_t fpcr;
        asm volatile("mrs %0, fpcr" : "=r"(fpcr));
        mOldFpcr = fpcr;
        asm volatile("msr fpcr, %0" : : "r"(fpcr | (1ULL << 24)));
#endif
#endif
    }

    ~ScopedNoDenormals() noexcept {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        _mm_setcsr(mOldMxcsr);
#elif defined(__aarch64__) || defined(_M_ARM64)
#if defined(_MSC_VER)
        _WriteStatusReg(ARM64_FPCR, mOldFpcr);
#elif defined(__GNUC__) || defined(__clang__)
        asm volatile("msr fpcr, %0" : : "r"(mOldFpcr));
#endif
#endif
    }

    ScopedNoDenormals(const ScopedNoDenormals&) = delete;
    ScopedNoDenormals& operator=(const ScopedNoDenormals&) = delete;
    ScopedNoDenormals(ScopedNoDenormals&&) = delete;
    ScopedNoDenormals& operator=(ScopedNoDenormals&&) = delete;

private:
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    unsigned int mOldMxcsr { 0 };
#elif defined(__aarch64__) || defined(_M_ARM64)
    uint64_t mOldFpcr { 0 };
#else
    int mDummy { 0 };
#endif
};

// ============================================================================
// Bitwise IEEE 754 Finite Checking
// Immune to compiler -ffast-math and /fp:fast branch eliminations.
// ============================================================================
[[nodiscard]] inline bool isFiniteBitwise(float val) noexcept {
    uint32_t bits;
    std::memcpy(&bits, &val, sizeof(float));
    // IEEE 754 single-precision: Exponent field [30:23] all-ones indicates NaN or Inf
    return (bits & 0x7F800000u) != 0x7F800000u;
}

[[nodiscard]] inline bool isNanOrInfBitwise(float val) noexcept {
    return !isFiniteBitwise(val);
}

// ============================================================================
// Software Denormal / NaN Flushing
// ============================================================================
[[nodiscard]] inline float flushDenormal(float val) noexcept {
    if (isNanOrInfBitwise(val)) [[unlikely]] {
        return 0.0f;
    }
    return (std::abs(val) < 1.0e-15f) ? 0.0f : val;
}

// ============================================================================
// Mathematical Constants
// ============================================================================
inline constexpr float kPi     = 3.14159265358979323846f;
inline constexpr float kTwoPi  = 6.28318530717958647692f;
inline constexpr float kHalfPi = 1.57079632679489661923f;
inline constexpr float kSqrt2  = 1.41421356237309504880f;

// ============================================================================
// Fast PRNG: 64-bit XorShift / SplitMix (Zero-Allocation, Thread-Safe instance)
// ============================================================================
class FastPrng {
public:
    constexpr explicit FastPrng(uint64_t seed = 0x504F4F50454EULL) noexcept : mState(seed ? seed : 0xDEADBEEFCAFEULL) {}

    void setSeed(uint64_t seed) noexcept {
        mState = seed ? seed : 0xDEADBEEFCAFEULL;
    }

    [[nodiscard]] inline uint64_t nextU64() noexcept {
        // 64-bit XorShift*
        uint64_t x = mState;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        mState = x;
        return x * 0x2545F4914F6CDD1DULL;
    }

    [[nodiscard]] inline float nextFloat01() noexcept {
        // Generate uniform float in [0.0, 1.0)
        const uint32_t val = static_cast<uint32_t>(nextU64() >> 40);
        return static_cast<float>(val) * (1.0f / 16777216.0f);
    }

    [[nodiscard]] inline float nextFloatSigned() noexcept {
        // Generate uniform float in [-1.0, 1.0)
        return nextFloat01() * 2.0f - 1.0f;
    }

private:
    uint64_t mState { 0x504F4F50454EULL };
};

// ============================================================================
// One-Pole Exponential Smoother (Sample-Rate Independent)
// ============================================================================
class OnePoleSmoother {
public:
    void setTimeConstant(double sampleRate, float timeSeconds) noexcept {
        if (timeSeconds <= 1.0e-5f || sampleRate < 1.0) {
            mCoeff = 1.0f;
        } else {
            mCoeff = 1.0f - std::exp(-1.0f / static_cast<float>(sampleRate * static_cast<double>(timeSeconds)));
        }
    }

    void reset(float initialValue = 0.0f) noexcept {
        mCurrentValue = initialValue;
        mTargetValue  = initialValue;
    }

    void setTarget(float target) noexcept {
        mTargetValue = target;
    }

    [[nodiscard]] inline float process() noexcept {
        mCurrentValue += mCoeff * (mTargetValue - mCurrentValue);
        mCurrentValue = flushDenormal(mCurrentValue);
        return mCurrentValue;
    }

    [[nodiscard]] inline float getCurrent() const noexcept {
        return mCurrentValue;
    }

private:
    float mCurrentValue { 0.0f };
    float mTargetValue  { 0.0f };
    float mCoeff        { 1.0f };
};

// ============================================================================
// One-Pole Lowpass Filter (Sample-Rate Independent)
// ============================================================================
class OnePoleLowpass {
public:
    void setCutoff(double sampleRate, float cutoffHz) noexcept {
        const float nyquist = static_cast<float>(sampleRate * 0.499);
        const float fc = std::clamp(cutoffHz, 10.0f, nyquist);
        const float omega = kTwoPi * fc / static_cast<float>(sampleRate);
        mAlpha = std::clamp(1.0f - std::exp(-omega), 0.0001f, 1.0f);
    }

    void reset(float val = 0.0f) noexcept {
        mZ1 = val;
    }

    [[nodiscard]] inline float process(float in) noexcept {
        mZ1 += mAlpha * (in - mZ1);
        mZ1 = flushDenormal(mZ1);
        return mZ1;
    }

private:
    float mZ1    { 0.0f };
    float mAlpha { 1.0f };
};

// ============================================================================
// Biquad Filter (Direct Form II Transposed)
// ============================================================================
class BiquadDirectForm2T {
public:
    void reset() noexcept {
        mS1 = 0.0f;
        mS2 = 0.0f;
    }

    void setBandpass(double sampleRate, float centerHz, float q) noexcept {
        const float nyquist = static_cast<float>(sampleRate * 0.499);
        const float f0 = std::clamp(centerHz, 20.0f, nyquist);
        const float safeQ = std::max(0.1f, q);
        const float w0 = kTwoPi * f0 / static_cast<float>(sampleRate);
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        const float alpha = sinw0 / (2.0f * safeQ);

        const float a0 = 1.0f + alpha;
        const float invA0 = 1.0f / a0;

        mB0 = (alpha) * invA0;
        mB1 = 0.0f;
        mB2 = (-alpha) * invA0;
        mA1 = (-2.0f * cosw0) * invA0;
        mA2 = (1.0f - alpha) * invA0;
    }

    void setLowpass(double sampleRate, float cutoffHz, float q = 0.7071f) noexcept {
        const float nyquist = static_cast<float>(sampleRate * 0.499);
        const float f0 = std::clamp(cutoffHz, 10.0f, nyquist);
        const float safeQ = std::max(0.1f, q);
        const float w0 = kTwoPi * f0 / static_cast<float>(sampleRate);
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        const float alpha = sinw0 / (2.0f * safeQ);

        const float a0 = 1.0f + alpha;
        const float invA0 = 1.0f / a0;

        mB0 = ((1.0f - cosw0) * 0.5f) * invA0;
        mB1 = (1.0f - cosw0) * invA0;
        mB2 = mB0;
        mA1 = (-2.0f * cosw0) * invA0;
        mA2 = (1.0f - alpha) * invA0;
    }

    [[nodiscard]] inline float process(float in) noexcept {
        const float out = mB0 * in + mS1;
        mS1 = mB1 * in - mA1 * out + mS2;
        mS2 = mB2 * in - mA2 * out;
        mS1 = flushDenormal(mS1);
        mS2 = flushDenormal(mS2);
        return flushDenormal(out);
    }

private:
    float mB0 { 1.0f }, mB1 { 0.0f }, mB2 { 0.0f };
    float mA1 { 0.0f }, mA2 { 0.0f };
    float mS1 { 0.0f }, mS2 { 0.0f };
};

// ============================================================================
// 4-Point 3rd-Order Catmull-Rom Hermite Interpolation for Fractional Delay
// ============================================================================
[[nodiscard]] inline float interpolateHermite4P3O(float ym1, float y0, float y1, float y2, float frac) noexcept {
    const float c0 = y0;
    const float c1 = 0.5f * (y1 - ym1);
    const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

// ============================================================================
// Decibel / Gain Conversions
// ============================================================================
[[nodiscard]] inline float dbToGain(float db) noexcept {
    return std::pow(10.0f, db * 0.05f);
}

[[nodiscard]] inline float gainToDb(float gain) noexcept {
    return (gain > 1.0e-5f) ? (20.0f * std::log10(gain)) : -100.0f;
}

} // namespace ppf42
