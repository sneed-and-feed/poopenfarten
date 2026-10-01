#pragma once

#include "DspDefines.h"
#include "DspMath.h"

#include <array>

namespace ppf42 {

class PorcelainConvolver {
public:
    static constexpr size_t kKernelSize = 1024;
    static constexpr size_t kKernelMask = kKernelSize - 1;

    PorcelainConvolver() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Process a single sample: takes cleft audio input, porcelain mix [0.0, 1.0], size [0.5, 2.0], model [0..3]
    [[nodiscard]] float processSample(float input, float mix, float size, int model) noexcept;

private:
    void rebuildKernel(int model, float size) noexcept;

    float mSampleRate { 48000.0f };
    std::array<float, kKernelSize> mHistoryBuffer {};
    std::array<float, kKernelSize> mKernel {};
    size_t mWriteIdx { 0 };

    int   mCurrentModel { -1 };
    float mCurrentSize  { -1.0f };
};

} // namespace ppf42
