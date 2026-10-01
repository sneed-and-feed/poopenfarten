#pragma once

#include "DspDefines.h"
#include "DspMath.h"

#include <array>

namespace ppf42 {

class CleftWaveguide {
public:
    static constexpr size_t kMaxDelaySamples = 2048;
    static constexpr size_t kDelayMask = kMaxDelaySamples - 1;

    CleftWaveguide() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Process a single sample: takes raw acoustic input and boundary damping parameter [0.0, 1.0]
    [[nodiscard]] float processSample(float input, float damping) noexcept;

    [[nodiscard]] float getResonanceEnergy() const noexcept { return mResonanceEnergy; }

private:
    float mSampleRate { 48000.0f };
    std::array<float, kMaxDelaySamples> mDelayLine {};
    size_t mWriteIdx { 0 };

    float mDelayedSample { 0.0f };
    float mDampingZ1     { 0.0f };
    float mResonanceEnergy { 0.0f };
};

} // namespace ppf42
