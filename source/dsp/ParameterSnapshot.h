#pragma once

#include <algorithm>
#include <cmath>

namespace ppf42 {

// ============================================================================
// ParameterSnapshot: Plain Old Data (POD) Snapshot of all 22 APVTS Parameters
// Audio thread receives this struct by value or reference, ensuring 0 locks.
// ============================================================================
struct ParameterSnapshot {
    // A. Mechanics (The Exciter)
    float pressure        { 0.70f }; // Colonic Drive [0.0, 1.0]
    float tension         { 0.50f }; // Tissue Elasticity [0.0, 1.0]
    float aperture        { 0.35f }; // Resting Gap [0.0, 1.0]
    float flutter         { 0.15f }; // Chaos / Asymmetry [0.0, 1.0]

    // B. Fluid Dynamics (The Wetness Engine)
    float viscosity       { 0.25f }; // Fluid Viscosity [0.0, 1.0]
    float moisture        { 0.20f }; // Moisture Level [0.0, 1.0]
    float droplet_rate    { 0.30f }; // Splatter Density [0.0, 1.0]

    // C. Cavity & Space
    float cleft_damping   { 0.50f }; // Boundary Damping [0.0, 1.0]
    float porcelain_mix   { 0.35f }; // Porcelain IR Mix [0.0, 1.0]
    float porcelain_size  { 1.00f }; // Cavity Volume [0.5, 2.0]
    int   porcelain_model { 1 };     // 0: Dry Chamber, 1: Ceramic Bowl, 2: Water Coupled, 3: Tiled Enclosure

    // D. Voice & Output
    int   voice_mode      { 0 };     // 0: Mono Legato, 1: Poly 8-Voice, 2: Stereo Unison Detune
    float glide_time      { 50.0f }; // Portamento glide slew in ms [0.0, 500.0]
    float sub_level       { -12.0f };// Sub Reinforcement in dB [-60.0, +6.0]
    float drive           { 0.15f }; // Saturation Drive [0.0, 1.0]
    float master_gain     { 0.00f }; // Master Gain in dB [-60.0, +12.0]

    // E. Performance Macros
    float macro_squeeze   { 0.50f }; // Gut Squeeze [0.0, 1.0]
    float macro_moisture  { 0.20f }; // Dietary Moisture [0.0, 1.0]

    // F. Pressure Envelope
    float env_attack      { 5.0f };   // Attack in ms [0.1, 200.0]
    float env_decay       { 350.0f }; // Decay in ms [10.0, 2000.0]
    float env_sustain     { 0.60f };  // Sustain level [0.0, 1.0]
    float env_release     { 120.0f }; // Release in ms [10.0, 2000.0]

    // G. Sample Bank & Pitcher Controls
    int   sample_index    { 3 };      // 0 to 7 (8 authentic sound effects, default 3: fartmeme)
    float sample_start    { 0.0f };   // Start offset [0.0, 1.0]
    int   sample_reverse  { 0 };      // 0: Forward, 1: Reverse
};

// ============================================================================
// Macro Coupling Transfer Function
// Applies coupled non-linear modulations for Gut Squeeze and Dietary Moisture
// ============================================================================
[[nodiscard]] inline ParameterSnapshot applyMacroCoupling(const ParameterSnapshot& in) noexcept {
    ParameterSnapshot out = in;
    const float ms = std::clamp(in.macro_squeeze, 0.0f, 1.0f);
    const float mm = std::clamp(in.macro_moisture, 0.0f, 1.0f);

    // Gut Squeeze Macro (Drive + Elasticity + Flutter)
    out.pressure = std::clamp(in.pressure * (0.5f + 0.8f * ms), 0.0f, 1.0f);
    out.tension  = std::clamp(in.tension * (0.7f + 0.6f * ms), 0.0f, 1.0f);
    out.flutter  = std::clamp(in.flutter + 0.4f * (ms * ms), 0.0f, 1.0f);

    // Dietary Moisture Macro (Viscosity + Moisture + Droplet Rate)
    out.viscosity    = std::clamp(in.viscosity * (0.4f + 1.2f * mm), 0.0f, 1.0f);
    out.moisture     = std::clamp(in.moisture * (0.2f + 1.6f * mm), 0.0f, 1.0f);
    out.droplet_rate = std::clamp(in.droplet_rate * std::pow(mm, 1.5f), 0.0f, 1.0f);

    return out;
}

} // namespace ppf42
