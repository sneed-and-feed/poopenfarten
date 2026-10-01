#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <string_view>

namespace ppf42 {

// ============================================================================
// SampleInfo: Static authentic audio asset metadata & PCM float data
// ============================================================================
struct SampleInfo {
    const char*        id;
    const char*        name;
    const char*        description;
    const float*       data;
    size_t             length;
    float              basePitchMidi; // Base root pitch: MIDI 48 (C3 = 130.81 Hz)
    double             sampleRate;    // 48000.0 Hz
};

inline constexpr size_t kNumEmbeddedSamples = 8;
inline constexpr float  kSampleBasePitchMidi = 48.0f; // C3
inline constexpr double kSampleBaseFreqHz   = 130.8127826502993;

class SampleBank {
public:
    static const SampleInfo& getSample(size_t index) noexcept;
    static constexpr size_t getSampleCount() noexcept { return kNumEmbeddedSamples; }
    static const std::array<SampleInfo, kNumEmbeddedSamples>& getAllSamples() noexcept;
};

} // namespace ppf42
