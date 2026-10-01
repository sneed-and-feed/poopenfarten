#pragma once

#include <cstddef>
#include <cstdint>

namespace ppf42 {

// ============================================================================
// Core Audio & Engine Configuration Defaults
// ============================================================================
inline constexpr double kDefaultSampleRate = 48000.0;
inline constexpr double kMinSampleRate     = 100.0;
inline constexpr double kMaxSampleRate     = 384000.0;

inline constexpr int kMaxBlockSize        = 8192;
inline constexpr int kDefaultBlockSize    = 512;

// Voice limits
inline constexpr int kMaxPolyVoices       = 8;
inline constexpr int kMaxUnisonVoices     = 4;

// Physical Modeling Frequency Boundaries (Aeroacoustic Calibrated Range)
inline constexpr float kMinOscFrequencyHz = 18.0f;  // Visceral Sub-Rumble (18 Hz)
inline constexpr float kMaxOscFrequencyHz = 350.0f; // High-Tension Squeaker (350 Hz)
inline constexpr float kDefaultFrequencyHz = 110.0f; // A2 note

// Physical Constants
inline constexpr float kAirDensityRho     = 1.204f;     // kg/m^3 (ambient air at 20 deg C)
inline constexpr float kFluidDensityRho   = 1000.0f;    // kg/m^3 (water/mucous density)
inline constexpr float kAtmosphericP0     = 101325.0f;  // Pa
inline constexpr float kAdiabaticGamma    = 1.4f;       // Polytropic index for air
inline constexpr float kSlitWidthW        = 0.025f;     // 25 mm anatomical slit width

// Master Dynamics Limits
inline constexpr float kMaxTruePeakCeiling = 1.00000f; // 0.0 dBFS ceiling
inline constexpr float kLimiterKnee        = 0.85000f; // Soft-knee transition threshold

// Telemetry & Scope Defaults
inline constexpr size_t kTelemetryScopeSamples = 128;
inline constexpr size_t kTelemetryQueueCapacity = 32;

} // namespace ppf42
