#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/ParameterSnapshot.h"
#include <array>
#include <string_view>

namespace ppf42 {

struct PresetDefinition {
    int index;
    const char* name;
    ParameterSnapshot snapshot;
};

inline constexpr size_t kNumPresets = 10;

extern const std::array<PresetDefinition, kNumPresets> kFactoryPresets;

int getNumPresets() noexcept;
const PresetDefinition& getPreset(int index) noexcept;
const char* getPresetName(int index) noexcept;

/**
 * Applies the preset at the specified index to the APVTS safely.
 */
void applyPreset(int index, juce::AudioProcessorValueTreeState& apvts);

} // namespace ppf42
