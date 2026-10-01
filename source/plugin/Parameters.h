#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/ParameterSnapshot.h"
#include <array>
#include <string_view>

namespace ppf42 {

namespace ParamIDs {
    inline constexpr std::string_view param_pressure        = "param_pressure";
    inline constexpr std::string_view param_tension         = "param_tension";
    inline constexpr std::string_view param_aperture        = "param_aperture";
    inline constexpr std::string_view param_flutter         = "param_flutter";
    inline constexpr std::string_view param_viscosity       = "param_viscosity";
    inline constexpr std::string_view param_moisture        = "param_moisture";
    inline constexpr std::string_view param_droplet_rate    = "param_droplet_rate";
    inline constexpr std::string_view param_cleft_damping   = "param_cleft_damping";
    inline constexpr std::string_view param_porcelain_mix   = "param_porcelain_mix";
    inline constexpr std::string_view param_porcelain_size  = "param_porcelain_size";
    inline constexpr std::string_view param_porcelain_model = "param_porcelain_model";
    inline constexpr std::string_view param_voice_mode      = "param_voice_mode";
    inline constexpr std::string_view param_glide_time      = "param_glide_time";
    inline constexpr std::string_view param_sub_level       = "param_sub_level";
    inline constexpr std::string_view param_drive           = "param_drive";
    inline constexpr std::string_view param_master_gain     = "param_master_gain";
    inline constexpr std::string_view macro_squeeze         = "macro_squeeze";
    inline constexpr std::string_view macro_moisture        = "macro_moisture";
    inline constexpr std::string_view param_env_attack      = "param_env_attack";
    inline constexpr std::string_view param_env_decay       = "param_env_decay";
    inline constexpr std::string_view param_env_sustain     = "param_env_sustain";
    inline constexpr std::string_view param_env_release     = "param_env_release";
}

struct ParamMetadata {
    const char* apvtsId;
    const char* name;
    const char* category;
    float minValue;
    float maxValue;
    float defaultValue;
    float skew;
    const char* unit;
    bool isChoice;
    int numChoices;
};

inline constexpr size_t kNumParams = 22;

extern const std::array<ParamMetadata, kNumParams> kParamRegistry;

/**
 * Creates the complete 22-parameter APVTS layout for PPF-42 DYNAMICS.
 */
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/**
 * Copies the current APVTS atomic values into a POD ParameterSnapshot.
 */
ParameterSnapshot createSnapshotFromAPVTS(const juce::AudioProcessorValueTreeState& apvts) noexcept;

} // namespace ppf42
