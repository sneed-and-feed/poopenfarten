#include "Parameters.h"

namespace ppf42 {

const std::array<ParamMetadata, kNumParams> kParamRegistry = {{
    { "param_pressure",        "Colonic Drive",      "Mechanics", 0.0f,  1.0f,    0.70f, 1.0f,  "%",   false, 0 },
    { "param_tension",         "Tissue Elasticity",  "Mechanics", 0.0f,  1.0f,    0.50f, 1.0f,  "%",   false, 0 },
    { "param_aperture",        "Resting Gap",        "Mechanics", 0.0f,  1.0f,    0.30f, 1.0f,  "%",   false, 0 },
    { "param_flutter",         "Chaos / Asymmetry",  "Mechanics", 0.0f,  1.0f,    0.20f, 1.0f,  "%",   false, 0 },
    { "param_viscosity",       "Fluid Viscosity",    "Fluid",     0.0f,  1.0f,    0.25f, 1.0f,  "%",   false, 0 },
    { "param_moisture",        "Moisture Level",     "Fluid",     0.0f,  1.0f,    0.20f, 1.0f,  "%",   false, 0 },
    { "param_droplet_rate",    "Splatter Density",   "Fluid",     0.0f,  1.0f,    0.30f, 1.0f,  "%",   false, 0 },
    { "param_cleft_damping",   "Boundary Damping",   "Cavity",    0.0f,  1.0f,    0.50f, 1.0f,  "%",   false, 0 },
    { "param_porcelain_mix",   "Porcelain IR Mix",   "Cavity",    0.0f,  1.0f,    0.35f, 1.0f,  "%",   false, 0 },
    { "param_porcelain_size",  "Cavity Volume",      "Cavity",    0.5f,  2.0f,    1.00f, 1.0f,  "x",   false, 0 },
    { "param_porcelain_model", "Resonant Model",     "Cavity",    0.0f,  3.0f,    1.00f, 1.0f,  "",    true,  4 },
    { "param_voice_mode",      "Voice Mode",         "Voice",     0.0f,  2.0f,    0.00f, 1.0f,  "",    true,  3 },
    { "param_glide_time",      "Portamento",         "Voice",     0.0f,  500.0f,  50.0f, 0.35f, "ms",  false, 0 },
    { "param_sub_level",       "Sub Reinforcement",  "Master",   -60.0f, 6.0f,   -12.0f, 2.0f,  "dB",  false, 0 },
    { "param_drive",           "Saturation Drive",   "Master",    0.0f,  1.0f,    0.15f, 1.0f,  "%",   false, 0 },
    { "param_master_gain",     "Master Output",      "Master",   -60.0f, 12.0f,   0.00f, 2.0f,  "dB",  false, 0 },
    { "macro_squeeze",         "Gut Squeeze",        "Macro",     0.0f,  1.0f,    0.50f, 1.0f,  "%",   false, 0 },
    { "macro_moisture",        "Dietary Moisture",   "Macro",     0.0f,  1.0f,    0.20f, 1.0f,  "%",   false, 0 },
    { "param_env_attack",      "Pressure Attack",    "Envelope",  0.1f,  200.0f,  5.0f,  0.30f, "ms",  false, 0 },
    { "param_env_decay",       "Pressure Decay",     "Envelope",  10.0f, 2000.0f, 350.0f,0.35f, "ms",  false, 0 },
    { "param_env_sustain",     "Pressure Sustain",   "Envelope",  0.0f,  1.0f,    0.60f, 1.0f,  "%",   false, 0 },
    { "param_env_release",     "Pressure Release",   "Envelope",  10.0f, 2000.0f, 120.0f,0.35f, "ms",  false, 0 },
    { "param_sample_index",    "Sample Slot",        "Sample",    0.0f,  7.0f,    3.0f,  1.0f,  "",    true,  8 },
    { "param_sample_start",    "Start Offset",       "Sample",    0.0f,  1.0f,    0.00f, 1.0f,  "%",   false, 0 },
    { "param_sample_reverse",  "Sample Reverse",     "Sample",    0.0f,  1.0f,    0.00f, 1.0f,  "",    true,  2 },
}};

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.reserve(kNumParams);

    // 1. Mechanics
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::param_pressure.data(), 1}, "Colonic Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f), 0.70f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::param_tension.data(), 1}, "Tissue Elasticity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f), 0.50f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::param_aperture.data(), 1}, "Resting Gap",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f), 0.30f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::param_flutter.data(), 1}, "Chaos / Asymmetry",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f), 0.20f));

    // 2. Fluid Dynamics
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::param_viscosity.data(), 1}, "Fluid Viscosity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f), 0.25f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::param_moisture.data(), 1}, "Moisture Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f), 0.20f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::param_droplet_rate.data(), 1}, "Splatter Density",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f), 0.30f));

    // 3. Cavity & Space
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::param_cleft_damping.data(), 1}, "Boundary Damping",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f), 0.50f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::param_porcelain_mix.data(), 1}, "Porcelain IR Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::param_porcelain_size.data(), 1}, "Cavity Volume",
        juce::NormalisableRange<float>(0.5f, 2.0f, 0.01f, 1.0f), 1.00f));

    juce::StringArray porcelainModels { "Dry Chamber", "Ceramic Bowl", "Water Coupled", "Tiled Enclosure" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ParamIDs::param_porcelain_model.data(), 1}, "Resonant Model",
        porcelainModels, 1));

    // 4. Voice & Output
    juce::StringArray voiceModes { "Mono Legato", "Poly 8-Voice", "Stereo Unison Detune" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ParamIDs::param_voice_mode.data(), 1}, "Voice Mode",
        voiceModes, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::param_glide_time.data(), 1}, "Portamento",
        juce::NormalisableRange<float>(0.0f, 500.0f, 0.1f, 0.35f), 50.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::param_sub_level.data(), 1}, "Sub Reinforcement",
        juce::NormalisableRange<float>(-60.0f, 6.0f, 0.1f, 2.0f), -12.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::param_drive.data(), 1}, "Saturation Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f), 0.15f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::param_master_gain.data(), 1}, "Master Output",
        juce::NormalisableRange<float>(-60.0f, 12.0f, 0.1f, 2.0f), 0.00f));

    // 5. Performance Macros
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::macro_squeeze.data(), 1}, "Gut Squeeze",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f), 0.50f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::macro_moisture.data(), 1}, "Dietary Moisture",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f), 0.20f));

    // 6. Pressure Envelope
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::param_env_attack.data(), 1}, "Pressure Attack",
        juce::NormalisableRange<float>(0.1f, 200.0f, 0.1f, 0.30f), 5.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::param_env_decay.data(), 1}, "Pressure Decay",
        juce::NormalisableRange<float>(10.0f, 2000.0f, 0.5f, 0.35f), 350.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::param_env_sustain.data(), 1}, "Pressure Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f), 0.60f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::param_env_release.data(), 1}, "Pressure Release",
        juce::NormalisableRange<float>(10.0f, 2000.0f, 0.5f, 0.35f), 120.0f));

    // 7. Sample Bank & Pitcher
    juce::StringArray sampleChoices {
        "1: Dry Staccato Rip",
        "2: Extended Gaseous Rip",
        "3: Porcelain Room Slam",
        "4: Iconic Wet Meme (4gcs5k8n-FY)",
        "5: Juicy Squelch Pop",
        "6: Flapping Flutter",
        "7: Crisp Percussive Slap",
        "8: Multiphase Splatter"
    };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ParamIDs::param_sample_index.data(), 1}, "Sample Slot",
        sampleChoices, 3));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::param_sample_start.data(), 1}, "Start Offset",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f), 0.00f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::param_sample_reverse.data(), 1}, "Sample Reverse",
        false));

    return { params.begin(), params.end() };
}

ParameterSnapshot createSnapshotFromAPVTS(const juce::AudioProcessorValueTreeState& apvts) noexcept
{
    ParameterSnapshot snap;

    auto getFloat = [&](std::string_view id, float defaultVal) noexcept -> float {
        if (auto* raw = apvts.getRawParameterValue(id.data()))
            return raw->load(std::memory_order_relaxed);
        return defaultVal;
    };

    auto getInt = [&](std::string_view id, int defaultVal) noexcept -> int {
        if (auto* raw = apvts.getRawParameterValue(id.data()))
            return static_cast<int>(std::round(raw->load(std::memory_order_relaxed)));
        return defaultVal;
    };

    snap.pressure        = getFloat(ParamIDs::param_pressure, 0.70f);
    snap.tension         = getFloat(ParamIDs::param_tension, 0.50f);
    snap.aperture        = getFloat(ParamIDs::param_aperture, 0.30f);
    snap.flutter         = getFloat(ParamIDs::param_flutter, 0.20f);
    snap.viscosity       = getFloat(ParamIDs::param_viscosity, 0.25f);
    snap.moisture        = getFloat(ParamIDs::param_moisture, 0.20f);
    snap.droplet_rate    = getFloat(ParamIDs::param_droplet_rate, 0.30f);
    snap.cleft_damping   = getFloat(ParamIDs::param_cleft_damping, 0.50f);
    snap.porcelain_mix   = getFloat(ParamIDs::param_porcelain_mix, 0.35f);
    snap.porcelain_size  = getFloat(ParamIDs::param_porcelain_size, 1.00f);
    snap.porcelain_model = getInt(ParamIDs::param_porcelain_model, 1);
    snap.voice_mode      = getInt(ParamIDs::param_voice_mode, 0);
    snap.glide_time      = getFloat(ParamIDs::param_glide_time, 50.0f);
    snap.sub_level       = getFloat(ParamIDs::param_sub_level, -12.0f);
    snap.drive           = getFloat(ParamIDs::param_drive, 0.15f);
    snap.master_gain     = getFloat(ParamIDs::param_master_gain, 0.00f);
    snap.macro_squeeze   = getFloat(ParamIDs::macro_squeeze, 0.50f);
    snap.macro_moisture  = getFloat(ParamIDs::macro_moisture, 0.20f);
    snap.env_attack      = getFloat(ParamIDs::param_env_attack, 5.0f);
    snap.env_decay       = getFloat(ParamIDs::param_env_decay, 350.0f);
    snap.env_sustain     = getFloat(ParamIDs::param_env_sustain, 0.60f);
    snap.env_release     = getFloat(ParamIDs::param_env_release, 120.0f);
    snap.sample_index    = getInt(ParamIDs::param_sample_index, 3);
    snap.sample_start    = getFloat(ParamIDs::param_sample_start, 0.0f);
    snap.sample_reverse  = getInt(ParamIDs::param_sample_reverse, 0);

    return snap;
}

} // namespace ppf42
