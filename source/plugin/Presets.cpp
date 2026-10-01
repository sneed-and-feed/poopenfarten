#include "Presets.h"
#include "Parameters.h"

namespace ppf42 {

const std::array<PresetDefinition, kNumPresets> kFactoryPresets = {{
    {
        0,
        "01 - Clean Continental Purr",
        {
            0.65f, 0.42f, 0.38f, 0.35f, // pressure, tension, aperture, flutter
            0.50f, 0.72f, 0.68f,        // viscosity, moisture, droplet_rate
            0.45f, 0.28f, 1.05f, 1,     // cleft_damping, porcelain_mix, porcelain_size, porcelain_model
            0, 35.0f, -3.0f, 0.22f, 0.00f, // voice_mode, glide_time, sub_level, drive, master_gain
            0.45f, 0.70f,               // macro_squeeze, macro_moisture
            8.0f, 650.0f, 0.40f, 180.0f // env_attack, env_decay, env_sustain, env_release
        }
    },
    {
        1,
        "02 - High-Tension Squeaker",
        {
            0.85f, 0.92f, 0.08f, 0.18f,
            0.12f, 0.15f, 0.10f,
            0.35f, 0.18f, 0.75f, 0,
            0, 25.0f, -20.0f, 0.28f, -1.00f,
            0.85f, 0.15f,
            2.0f, 220.0f, 0.65f, 55.0f
        }
    },
    {
        2,
        "03 - Viscous Multiphase Splatter",
        {
            0.78f, 0.40f, 0.45f, 0.40f,
            0.85f, 0.80f, 0.82f,
            0.42f, 0.42f, 1.15f, 2,
            0, 50.0f, -4.0f, 0.28f, 0.00f,
            0.60f, 0.85f,
            5.0f, 550.0f, 0.42f, 160.0f
        }
    },
    {
        3,
        "04 - Visceral Sub-Rumble (18 Hz)",
        {
            0.90f, 0.12f, 0.58f, 0.30f,
            0.35f, 0.28f, 0.22f,
            0.75f, 0.48f, 1.55f, 1,
            0, 75.0f, 3.0f, 0.42f, 0.50f,
            0.70f, 0.35f,
            18.0f, 850.0f, 0.75f, 280.0f
        }
    },
    {
        4,
        "05 - Flutter-Tongue Stutter",
        {
            0.75f, 0.48f, 0.32f, 0.95f,
            0.28f, 0.32f, 0.28f,
            0.42f, 0.28f, 1.05f, 0,
            0, 30.0f, -9.0f, 0.24f, 0.00f,
            0.80f, 0.35f,
            5.0f, 480.0f, 0.55f, 90.0f
        }
    },
    {
        5,
        "06 - Wet Porcelain Slam",
        {
            0.82f, 0.36f, 0.42f, 0.35f,
            0.72f, 0.78f, 0.65f,
            0.32f, 0.72f, 1.35f, 1,
            0, 45.0f, -2.0f, 0.32f, 0.00f,
            0.65f, 0.80f,
            8.0f, 620.0f, 0.48f, 210.0f
        }
    },
    {
        6,
        "07 - Micro-Puff Staccato",
        {
            0.62f, 0.62f, 0.22f, 0.12f,
            0.18f, 0.15f, 0.18f,
            0.65f, 0.12f, 0.85f, 0,
            0, 0.0f, -15.0f, 0.10f, 1.50f,
            0.35f, 0.18f,
            0.5f, 70.0f, 0.00f, 20.0f
        }
    },
    {
        7,
        "08 - Extended Gaseous Drift",
        {
            0.58f, 0.32f, 0.52f, 0.42f,
            0.22f, 0.18f, 0.12f,
            0.48f, 0.32f, 1.05f, 3,
            0, 110.0f, -6.0f, 0.18f, 0.00f,
            0.40f, 0.22f,
            35.0f, 1750.0f, 0.70f, 380.0f
        }
    },
    {
        8,
        "09 - Unison Twin Cannons",
        {
            0.85f, 0.40f, 0.38f, 0.52f,
            0.35f, 0.32f, 0.35f,
            0.38f, 0.48f, 1.20f, 1,
            2, 45.0f, 0.0f, 0.38f, -2.00f,
            0.75f, 0.35f,
            10.0f, 520.0f, 0.68f, 160.0f
        }
    },
    {
        9,
        "10 - The Brown Note 808",
        {
            0.95f, 0.10f, 0.28f, 0.18f,
            0.25f, 0.18f, 0.15f,
            0.88f, 0.25f, 1.55f, 1,
            0, 15.0f, 6.0f, 0.52f, 1.00f,
            0.90f, 0.20f,
            1.0f, 1150.0f, 0.35f, 260.0f
        }
    }
}};

int getNumPresets() noexcept
{
    return static_cast<int>(kNumPresets);
}

const PresetDefinition& getPreset(int index) noexcept
{
    const int idx = std::clamp(index, 0, static_cast<int>(kNumPresets) - 1);
    return kFactoryPresets[static_cast<size_t>(idx)];
}

const char* getPresetName(int index) noexcept
{
    return getPreset(index).name;
}

void applyPreset(int index, juce::AudioProcessorValueTreeState& apvts)
{
    const auto& p = getPreset(index);
    const auto& s = p.snapshot;

    auto setParam = [&](std::string_view id, float val) {
        if (auto* param = apvts.getParameter(id.data()))
        {
            param->setValueNotifyingHost(param->convertTo0to1(val));
        }
    };

    setParam(ParamIDs::param_pressure,        s.pressure);
    setParam(ParamIDs::param_tension,         s.tension);
    setParam(ParamIDs::param_aperture,        s.aperture);
    setParam(ParamIDs::param_flutter,         s.flutter);
    setParam(ParamIDs::param_viscosity,       s.viscosity);
    setParam(ParamIDs::param_moisture,        s.moisture);
    setParam(ParamIDs::param_droplet_rate,    s.droplet_rate);
    setParam(ParamIDs::param_cleft_damping,   s.cleft_damping);
    setParam(ParamIDs::param_porcelain_mix,   s.porcelain_mix);
    setParam(ParamIDs::param_porcelain_size,  s.porcelain_size);
    setParam(ParamIDs::param_porcelain_model, static_cast<float>(s.porcelain_model));
    setParam(ParamIDs::param_voice_mode,      static_cast<float>(s.voice_mode));
    setParam(ParamIDs::param_glide_time,      s.glide_time);
    setParam(ParamIDs::param_sub_level,       s.sub_level);
    setParam(ParamIDs::param_drive,           s.drive);
    setParam(ParamIDs::param_master_gain,     s.master_gain);
    setParam(ParamIDs::macro_squeeze,         s.macro_squeeze);
    setParam(ParamIDs::macro_moisture,        s.macro_moisture);
    setParam(ParamIDs::param_env_attack,      s.env_attack);
    setParam(ParamIDs::param_env_decay,       s.env_decay);
    setParam(ParamIDs::param_env_sustain,     s.env_sustain);
    setParam(ParamIDs::param_env_release,     s.env_release);
}

} // namespace ppf42
