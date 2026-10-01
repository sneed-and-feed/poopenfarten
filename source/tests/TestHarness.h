#pragma once

#include "dsp/DspDefines.h"
#include "dsp/DspMath.h"

#include <iostream>
#include <vector>
#include <complex>
#include <cmath>
#include <string>
#include <functional>
#include <algorithm>
#include <chrono>
#include <iomanip>

namespace ppf42::test {

inline int gCurrentTestAssertFailures = 0;

#if defined(_MSC_VER)
#define TEST_ASSERT(cond, msg) do { \
    __pragma(warning(push)) \
    __pragma(warning(disable: 4127)) \
    if (!(cond)) { \
        std::cerr << "      [ASSERTION FAILED] " << __FILE__ << ":" << __LINE__ << " -> " << (msg) << "\n"; \
        ++ppf42::test::gCurrentTestAssertFailures; \
    } \
    __pragma(warning(pop)) \
} while (0)
#else
#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "      [ASSERTION FAILED] " << __FILE__ << ":" << __LINE__ << " -> " << (msg) << "\n"; \
        ++ppf42::test::gCurrentTestAssertFailures; \
    } \
} while (0)
#endif

#define TEST_ASSERT_NEAR(val, expected, tol, msg) do { \
    double v_ = static_cast<double>(val); \
    double exp_ = static_cast<double>(expected); \
    double diff_ = std::abs(v_ - exp_); \
    if (diff_ > (tol)) { \
        std::cerr << "      [ASSERTION FAILED] " << __FILE__ << ":" << __LINE__ << " -> " << (msg) \
                  << " (actual=" << v_ << ", expected=" << exp_ << ", diff=" << diff_ << ", tol=" << (tol) << ")\n"; \
        ++ppf42::test::gCurrentTestAssertFailures; \
    } \
} while (0)

struct TestCase {
    std::string tier;
    std::string id;
    std::string name;
    std::function<bool()> run;
};

inline std::vector<TestCase>& getTestRegistry() {
    static std::vector<TestCase> registry;
    return registry;
}

inline void registerTest(const std::string& tier, const std::string& id, const std::string& name, std::function<bool()> fn) {
    getTestRegistry().push_back({tier, id, name, std::move(fn)});
}

#define REGISTER_TEST(tier, id, name, func) \
    static const bool id##_reg = []() { \
        ppf42::test::registerTest(tier, #id, name, func); \
        return true; \
    }()

// ============================================================================
// Test Utilities: FFT, Windowing, Peak Frequency Detection
// ============================================================================
inline void fft(std::vector<std::complex<double>>& a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * kPi / len;
        const std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (size_t j = 0; j < len / 2; ++j) {
                const std::complex<double> u = a[i + j];
                const std::complex<double> v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

inline double findPeakFrequency(const std::vector<float>& signal, double sampleRate, size_t startSample, size_t numSamples, double minF, double maxF) {
    size_t fftSize = 1;
    while (fftSize * 2 <= numSamples) fftSize *= 2;
    if (fftSize < 256) fftSize = 256;
    if (startSample + fftSize > signal.size()) {
        if (signal.size() <= fftSize) startSample = 0;
        else startSample = signal.size() - fftSize;
    }

    std::vector<std::complex<double>> buffer(fftSize);
    for (size_t i = 0; i < fftSize; ++i) {
        // 4-term Blackman-Harris window
        constexpr double a0 = 0.35875, a1 = 0.48829, a2 = 0.14128, a3 = 0.01168;
        const double t = 2.0 * kPi * i / (fftSize - 1);
        const double win = a0 - a1 * std::cos(t) + a2 * std::cos(2.0 * t) - a3 * std::cos(3.0 * t);
        const size_t idx = startSample + i;
        const float val = (idx < signal.size()) ? signal[idx] : 0.0f;
        buffer[i] = static_cast<double>(val) * win;
    }

    fft(buffer);

    const double binWidth = sampleRate / static_cast<double>(fftSize);
    const size_t minBin = std::max(size_t{1}, static_cast<size_t>(minF / binWidth));
    const size_t maxBin = std::min(fftSize / 2 - 2, static_cast<size_t>(maxF / binWidth) + 1);

    size_t bestBin = minBin;
    double maxMag = 0.0;
    for (size_t i = minBin; i <= maxBin; ++i) {
        const double mag = std::norm(buffer[i]);
        if (mag > maxMag) {
            maxMag = mag;
            bestBin = i;
        }
    }

    // Quadratic interpolation around peak bin for sub-bin frequency accuracy
    if (bestBin > 0 && bestBin + 1 < fftSize / 2) {
        const double y0 = std::abs(buffer[bestBin - 1]);
        const double y1 = std::abs(buffer[bestBin]);
        const double y2 = std::abs(buffer[bestBin + 1]);
        const double delta = 0.5 * (y0 - y2) / (y0 - 2.0 * y1 + y2 + 1e-12);
        return (static_cast<double>(bestBin) + delta) * binWidth;
    }

    return static_cast<double>(bestBin) * binWidth;
}

} // namespace ppf42::test
