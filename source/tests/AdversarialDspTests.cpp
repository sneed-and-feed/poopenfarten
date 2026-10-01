#include "TestHarness.h"
#include "AllocationTracker.h"
#include "dsp/DspDefines.h"
#include "dsp/DspMath.h"
#include "dsp/DspEngine.h"
#include "dsp/BoundedSaturator.h"
#include "dsp/TelemetryRingBuffer.h"

#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <array>
#include <cmath>
#include <iostream>
#include <algorithm>

namespace ppf42::test {

// Definition of the 10 anatomical factory preset snapshots for headless verification
static const std::array<ParameterSnapshot, 10> kAdversarialFactoryPresets = {{
    // 01 - Clean Continental Purr
    { 0.65f, 0.42f, 0.38f, 0.35f, 0.50f, 0.72f, 0.68f, 0.45f, 0.28f, 1.05f, 1, 0, 35.0f, -3.0f, 0.22f, 0.00f, 0.45f, 0.70f, 8.0f, 650.0f, 0.40f, 180.0f },
    // 02 - High-Tension Squeaker
    { 0.85f, 0.92f, 0.08f, 0.18f, 0.12f, 0.15f, 0.10f, 0.35f, 0.18f, 0.75f, 0, 0, 25.0f, -20.0f, 0.28f, -1.00f, 0.85f, 0.15f, 2.0f, 220.0f, 0.65f, 55.0f },
    // 03 - Viscous Multiphase Splatter
    { 0.78f, 0.40f, 0.45f, 0.40f, 0.85f, 0.80f, 0.82f, 0.42f, 0.42f, 1.15f, 2, 0, 50.0f, -4.0f, 0.28f, 0.00f, 0.60f, 0.85f, 5.0f, 550.0f, 0.42f, 160.0f },
    // 04 - Visceral Sub-Rumble (18 Hz)
    { 0.90f, 0.12f, 0.58f, 0.30f, 0.35f, 0.28f, 0.22f, 0.75f, 0.48f, 1.55f, 1, 0, 75.0f, 3.0f, 0.42f, 0.50f, 0.70f, 0.35f, 18.0f, 850.0f, 0.75f, 280.0f },
    // 05 - Flutter-Tongue Stutter
    { 0.75f, 0.48f, 0.32f, 0.95f, 0.28f, 0.32f, 0.28f, 0.42f, 0.28f, 1.05f, 0, 0, 30.0f, -9.0f, 0.24f, 0.00f, 0.80f, 0.35f, 5.0f, 480.0f, 0.55f, 90.0f },
    // 06 - Wet Porcelain Slam
    { 0.82f, 0.36f, 0.42f, 0.35f, 0.72f, 0.78f, 0.65f, 0.32f, 0.72f, 1.35f, 1, 0, 45.0f, -2.0f, 0.32f, 0.00f, 0.65f, 0.80f, 8.0f, 620.0f, 0.48f, 210.0f },
    // 07 - Micro-Puff Staccato
    { 0.62f, 0.62f, 0.22f, 0.12f, 0.18f, 0.15f, 0.18f, 0.65f, 0.12f, 0.85f, 0, 0, 0.0f, -15.0f, 0.10f, 1.50f, 0.35f, 0.18f, 0.5f, 70.0f, 0.00f, 20.0f },
    // 08 - Extended Gaseous Drift
    { 0.58f, 0.32f, 0.52f, 0.42f, 0.22f, 0.18f, 0.12f, 0.48f, 0.32f, 1.05f, 3, 0, 110.0f, -6.0f, 0.18f, 0.00f, 0.40f, 0.22f, 35.0f, 1750.0f, 0.70f, 380.0f },
    // 09 - Unison Twin Cannons
    { 0.85f, 0.40f, 0.38f, 0.52f, 0.35f, 0.32f, 0.35f, 0.38f, 0.48f, 1.20f, 1, 2, 45.0f, 0.0f, 0.38f, -2.00f, 0.75f, 0.35f, 10.0f, 520.0f, 0.68f, 160.0f },
    // 10 - The Brown Note 808
    { 0.95f, 0.10f, 0.28f, 0.18f, 0.25f, 0.18f, 0.15f, 0.88f, 0.25f, 1.55f, 1, 0, 15.0f, 6.0f, 0.52f, 1.00f, 0.90f, 0.20f, 1.0f, 1150.0f, 0.35f, 260.0f }
}};

// Helper: Verify sample buffer for IEEE-754 correctness, no NaN/Inf, no denormals, strict <= 0.0 dBFS ceiling
static inline void auditSampleBlock(const float* left, const float* right, int numSamples, const char* contextTag) {
    for (int i = 0; i < numSamples; ++i) {
        const float l = left[i];
        const float r = right[i];

        if (!isFiniteBitwise(l) || !isFiniteBitwise(r)) {
            TEST_ASSERT(false, std::string(contextTag) + ": Non-finite (NaN/Inf) audio sample detected!");
        }

        if (std::abs(l) > 1.00001f || std::abs(r) > 1.00001f) {
            TEST_ASSERT(false, std::string(contextTag) + ": Peak limiter ceiling exceeded (> 0.0 dBFS)!");
        }

        // Bitwise denormal / subnormal detection (non-zero value < 1.175494e-38)
        uint32_t bitsL, bitsR;
        std::memcpy(&bitsL, &l, sizeof(float));
        std::memcpy(&bitsR, &r, sizeof(float));

        const uint32_t expL = (bitsL & 0x7F800000u) >> 23;
        const uint32_t mantL = bitsL & 0x007FFFFFu;
        const uint32_t expR = (bitsR & 0x7F800000u) >> 23;
        const uint32_t mantR = bitsR & 0x007FFFFFu;

        if (expL == 0 && mantL != 0) {
            TEST_ASSERT(false, std::string(contextTag) + ": Denormal/subnormal float detected on Left channel!");
        }
        if (expR == 0 && mantR != 0) {
            TEST_ASSERT(false, std::string(contextTag) + ": Denormal/subnormal float detected on Right channel!");
        }
    }
}

// ============================================================================
// 1. T5_Challenger_RapidPresetSwitching_ActiveNotes
// Stress-tests rapid switching across all 10 presets during active note playback
// ============================================================================
bool test_T5_Challenger_RapidPresetSwitching_ActiveNotes() {
    constexpr double kSampleRates[] = { 44100.0, 48000.0, 96000.0 };

    for (double sr : kSampleRates) {
        DspEngine engine;
        engine.prepare(sr, 128);

        // Start with active polyphonic chord
        std::vector<MidiEvent> initialNotes = {
            { 0, 0x90, 36, 110 }, // C2
            { 0, 0x90, 48, 100 }, // C3
            { 0, 0x90, 55, 95 },  // G3
            { 0, 0x90, 60, 90 }   // C4
        };

        std::vector<float> left(128);
        std::vector<float> right(128);

        engine.process(left.data(), right.data(), 128, kAdversarialFactoryPresets[0], initialNotes.data(), static_cast<int>(initialNotes.size()));
        auditSampleBlock(left.data(), right.data(), 128, "Initial Notes");

        FastPrng prng(0xABCDEF012345ULL);
        constexpr int kNumBlocks = 1000; // 128,000 samples per sample rate
        int currentPreset = 0;

        for (int block = 0; block < kNumBlocks; ++block) {
            // Rapid preset switch: change preset every 1, 2, or 3 blocks
            if ((block % 2) == 0) {
                currentPreset = static_cast<int>(prng.nextU64() % 10);
            }

            // Occasionally inject pitch bend or aftertouch to stress modulation
            MidiEvent midiEv[2];
            int midiCount = 0;
            if ((block % 10) == 0) {
                midiEv[midiCount++] = { 0, 0xE0, static_cast<uint8_t>(prng.nextU64() & 0x7F), static_cast<uint8_t>(prng.nextU64() & 0x7F) };
            }
            if ((block % 15) == 0) {
                midiEv[midiCount++] = { 0, 0xD0, static_cast<uint8_t>(prng.nextU64() & 0x7F), 0 };
            }

            const auto& snapshot = kAdversarialFactoryPresets[currentPreset];
            engine.process(left.data(), right.data(), 128, snapshot, midiEv, midiCount);

            auditSampleBlock(left.data(), right.data(), 128, "Rapid Preset Switching");
        }
    }

    return gCurrentTestAssertFailures == 0;
}

// ============================================================================
// 2. T5_Challenger_ExtremeParameters_OverdriveAndResonance
// Stress-tests maximum colonic drive, gut squeeze, moisture, cavity resonance
// ============================================================================
bool test_T5_Challenger_ExtremeParameters_OverdriveAndResonance() {
    DspEngine engine;
    engine.prepare(48000.0, 256);

    // Worst-case parameter snapshot: all energy boosters maximized
    ParameterSnapshot extremeP;
    extremeP.pressure        = 1.0f;     // Max colonic drive (1500 Pa)
    extremeP.tension         = 1.0f;     // Max tissue stiffness
    extremeP.aperture        = 1.0f;     // Max resting aperture
    extremeP.flutter         = 1.0f;     // Max flutter chaos
    extremeP.viscosity       = 1.0f;     // Max fluid mass loading
    extremeP.moisture        = 1.0f;     // Max bubble and droplet density
    extremeP.droplet_rate    = 1.0f;     // Max Poisson droplet rate
    extremeP.cleft_damping   = 0.0f;     // Zero damping = MAX resonant feedback in waveguide
    extremeP.porcelain_mix   = 1.0f;     // 100% Wet ceramic cavity convolver
    extremeP.porcelain_size  = 2.0f;     // Max 2.0x cavity volume
    extremeP.porcelain_model = 1;        // Ceramic Bowl modal resonator
    extremeP.voice_mode      = 1;        // Polyphonic 8-Voice Mode
    extremeP.glide_time      = 0.0f;     // Instant pitch shifts
    extremeP.sub_level       = 6.0f;     // +6 dB low-end boost
    extremeP.drive           = 1.0f;     // Max 100% saturation drive
    extremeP.master_gain     = 12.0f;    // +12 dB extreme master gain boost!
    extremeP.macro_squeeze   = 1.0f;     // 100% Gut Squeeze
    extremeP.macro_moisture  = 1.0f;     // 100% Dietary Moisture
    extremeP.env_attack      = 0.1f;     // Instant 0.1 ms attack
    extremeP.env_decay       = 2000.0f;  // Max decay
    extremeP.env_sustain     = 1.0f;     // 100% sustain
    extremeP.env_release     = 2000.0f;

    // Trigger full 8-voice polyphonic cluster in the deep bass register
    std::vector<MidiEvent> cluster;
    for (int n = 0; n < 8; ++n) {
        cluster.push_back({ 0, 0x90, static_cast<uint8_t>(24 + n * 3), 127 }); // Velocity 127
    }

    std::vector<float> left(256);
    std::vector<float> right(256);

    engine.process(left.data(), right.data(), 256, extremeP, cluster.data(), static_cast<int>(cluster.size()));
    auditSampleBlock(left.data(), right.data(), 256, "Extreme Parameters Init");

    // Continuous rendering for 500 blocks = 128,000 samples under full overdrive
    float maxObservedPeak = 0.0f;
    for (int block = 0; block < 500; ++block) {
        // Toggle between ceramic bowl (1) and tiled enclosure (3) every 50 blocks
        extremeP.porcelain_model = ((block / 50) % 2 == 0) ? 1 : 3;

        engine.process(left.data(), right.data(), 256, extremeP, nullptr, 0);

        for (int i = 0; i < 256; ++i) {
            maxObservedPeak = std::max(maxObservedPeak, std::max(std::abs(left[i]), std::abs(right[i])));
        }

        auditSampleBlock(left.data(), right.data(), 256, "Extreme Overdrive Continuous");
    }

    TEST_ASSERT(maxObservedPeak > 0.5f, "DSP engine should produce substantial energy under max drive");
    TEST_ASSERT(maxObservedPeak <= 1.00001f, "Peak limiter failed to contain extreme overdrive <= 1.0 (0.0 dBFS)!");

    return gCurrentTestAssertFailures == 0;
}

// ============================================================================
// 3. T5_Challenger_PeakLimiter_StrictCeilingAndOverloadStress
// White-box unit stress test on BoundedSaturator limiter and saturator
// ============================================================================
bool test_T5_Challenger_PeakLimiter_StrictCeilingAndOverloadStress() {
    // A. Sub-knee linearity: for |x| <= 0.85, output must equal input
    for (float x = -0.85f; x <= 0.85f; x += 0.01f) {
        const float out = BoundedSaturator::limitSample(x, 1.0f);
        TEST_ASSERT_NEAR(out, x, 1.0e-5, "Limiter altered linear signal below knee");
    }

    // B. Super-ceiling saturation: for |x| >= 1.0, output must be clamped strictly to 1.0
    for (float x = 1.0f; x <= 1000.0f; x += 1.5f) {
        const float outPos = BoundedSaturator::limitSample(x, 1.0f);
        const float outNeg = BoundedSaturator::limitSample(-x, 1.0f);

        TEST_ASSERT_NEAR(outPos, 1.0f, 1.0e-6, "Positive overload exceeded 1.000000");
        TEST_ASSERT_NEAR(outNeg, -1.0f, 1.0e-6, "Negative overload fell below -1.000000");
    }

    // C. Massive gain factor: input with +40 dB gain
    const float hugeGain = dbToGain(40.0f); // 100x gain
    for (float in = -10.0f; in <= 10.0f; in += 0.1f) {
        const float out = BoundedSaturator::limitSample(in, hugeGain);
        TEST_ASSERT(out >= -1.0f && out <= 1.0f, "Limiter output exceeded [-1, +1] with 40 dB gain");
        TEST_ASSERT(isFiniteBitwise(out), "Limiter output non-finite with 40 dB gain");
    }

    // D. Non-finite input protection: NaN and Inf must flush to clean 0.0f
    const float qnan = std::numeric_limits<float>::quiet_NaN();
    const float snan = std::numeric_limits<float>::signaling_NaN();
    const float pinf = std::numeric_limits<float>::infinity();
    const float ninf = -std::numeric_limits<float>::infinity();

    TEST_ASSERT(BoundedSaturator::limitSample(qnan, 1.0f) == 0.0f, "Quiet NaN not flushed to 0 by limiter");
    TEST_ASSERT(BoundedSaturator::limitSample(snan, 1.0f) == 0.0f, "Signaling NaN not flushed to 0 by limiter");
    TEST_ASSERT(BoundedSaturator::limitSample(pinf, 1.0f) == 0.0f, "+Inf not flushed to 0 by limiter");
    TEST_ASSERT(BoundedSaturator::limitSample(ninf, 1.0f) == 0.0f, "-Inf not flushed to 0 by limiter");

    // E. Subnormals flushing
    const float denorm = 1.0e-39f;
    const float outDenorm = BoundedSaturator::limitSample(denorm, 1.0f);
    TEST_ASSERT(outDenorm == 0.0f, "Subnormal not flushed to zero by limiter");

    return gCurrentTestAssertFailures == 0;
}

// ============================================================================
// 4. T5_Challenger_TelemetryRingBuffer_SaturationStress
// High-contention SPSC ring buffer stress test (overflow and underflow)
// ============================================================================
bool test_T5_Challenger_TelemetryRingBuffer_SaturationStress() {
    TelemetryRingBuffer ring;

    std::atomic<bool> producerDone { false };
    std::atomic<int> framesPushed { 0 };
    std::atomic<int> framesDropped { 0 };
    std::atomic<int> framesPopped { 0 };
    float lastPressureSeen = -1.0f;

    constexpr int kTotalPushTarget = 50000;

    // Thread 1: High-Speed Producer (Simulating audio thread pushing frames with sequence numbers)
    std::thread producer([&]() {
        VisualizerFrame frame {};
        for (int i = 0; i < kTotalPushTarget; ++i) {
            frame.colonicPressure = static_cast<float>(i);
            if (ring.push(frame)) {
                framesPushed.fetch_add(1, std::memory_order_relaxed);
            } else {
                framesDropped.fetch_add(1, std::memory_order_relaxed);
            }
            if ((i % 32) == 0) {
                std::this_thread::yield();
            }
        }
        producerDone.store(true, std::memory_order_release);
    });

    // Thread 2: Intermittent Consumer (Simulating UI thread reading frames in bursts)
    std::thread consumer([&]() {
        VisualizerFrame frame {};
        int cycle = 0;
        while (true) {
            while (ring.pop(frame)) {
                framesPopped.fetch_add(1, std::memory_order_relaxed);
                const float p = frame.colonicPressure;
                if (p < lastPressureSeen) {
                    TEST_ASSERT(false, "Telemetry frames popped out-of-order!");
                }
                lastPressureSeen = p;
            }
            if (producerDone.load(std::memory_order_acquire)) {
                // Drain any remaining frames
                while (ring.pop(frame)) {
                    framesPopped.fetch_add(1, std::memory_order_relaxed);
                    lastPressureSeen = frame.colonicPressure;
                }
                break;
            }
            if (++cycle % 50 == 0) {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    const int pushed = framesPushed.load();
    const int dropped = framesDropped.load();
    const int popped = framesPopped.load();

    TEST_ASSERT(pushed + dropped == kTotalPushTarget, "Total push attempts must equal pushed + dropped");
    TEST_ASSERT(pushed > 100, "Producer should have successfully pushed frames");
    TEST_ASSERT(dropped > 0, "Buffer overflow dropping should have been triggered under saturation");
    TEST_ASSERT(popped == pushed, "All successfully pushed frames must be popped");

    return gCurrentTestAssertFailures == 0;
}

// ============================================================================
// 5. T5_Challenger_DecayToSilence_ZeroDenormals
// Verifies that a ringing acoustic body and reverb tail decays into true zero
// ============================================================================
bool test_T5_Challenger_DecayToSilence_ZeroDenormals() {
    DspEngine engine;
    engine.prepare(48000.0, 256);

    ParameterSnapshot p = kAdversarialFactoryPresets[5]; // Wet Porcelain Slam (heavy reverb)
    p.porcelain_mix = 0.8f;
    p.cleft_damping = 0.2f;

    // Trigger powerful short note burst
    MidiEvent noteOn { 0, 0x90, 48, 127 };
    std::vector<float> left(256);
    std::vector<float> right(256);

    engine.process(left.data(), right.data(), 256, p, &noteOn, 1);

    // Note Off
    MidiEvent noteOff { 0, 0x80, 48, 0 };
    engine.process(left.data(), right.data(), 256, p, &noteOff, 1);

    // Allow engine to decay for 200,000 samples (~4.16 seconds)
    constexpr int kTotalDecaySamples = 200000;
    int processed = 0;

    while (processed < kTotalDecaySamples) {
        engine.process(left.data(), right.data(), 256, p, nullptr, 0);
        auditSampleBlock(left.data(), right.data(), 256, "Decay to Silence");
        processed += 256;
    }

    // After 200,000 samples, output should have decayed into pure silence
    const float tailL = left[255];
    const float tailR = right[255];
    TEST_ASSERT(std::abs(tailL) < 1.0e-5f, "Left channel did not decay to silence after 4.16 seconds");
    TEST_ASSERT(std::abs(tailR) < 1.0e-5f, "Right channel did not decay to silence after 4.16 seconds");

    return gCurrentTestAssertFailures == 0;
}

// ============================================================================
// 6. T5_Challenger_PresetCatalogFidelity_All10Presets
// Verifies all 10 presets render clean, audible, bounded audio with no dead presets
// ============================================================================
bool test_T5_Challenger_PresetCatalogFidelity_All10Presets() {
    DspEngine engine;
    engine.prepare(48000.0, 128);

    std::vector<float> left(128);
    std::vector<float> right(128);

    for (size_t i = 0; i < kAdversarialFactoryPresets.size(); ++i) {
        engine.reset();
        const auto& p = kAdversarialFactoryPresets[i];

        // Trigger note 48 (C3)
        MidiEvent noteOn { 0, 0x90, 48, 100 };
        engine.process(left.data(), right.data(), 128, p, &noteOn, 1);

        float rmsAccum = 0.0f;
        constexpr int kTestBlocks = 40; // 5120 samples (~106 ms)
        for (int b = 0; b < kTestBlocks; ++b) {
            engine.process(left.data(), right.data(), 128, p, nullptr, 0);
            auditSampleBlock(left.data(), right.data(), 128, "Preset Fidelity Audit");

            for (int s = 0; s < 128; ++s) {
                rmsAccum += left[s] * left[s] + right[s] * right[s];
            }
        }

        const float rms = std::sqrt(rmsAccum / (kTestBlocks * 128 * 2));
        TEST_ASSERT(rms > 0.0005f, "Preset rendered near-total silence during active note playback");
    }

    return gCurrentTestAssertFailures == 0;
}

// Register all Tier 5 Adversarial Tests
REGISTER_TEST("Tier5", T5_RapidPresetSwitching_ActiveNotes, "Rapid Preset Switching Under Active Load", test_T5_Challenger_RapidPresetSwitching_ActiveNotes);
REGISTER_TEST("Tier5", T5_ExtremeParameters_OverdriveAndResonance, "Extreme Parameters Overdrive & Resonance Stress", test_T5_Challenger_ExtremeParameters_OverdriveAndResonance);
REGISTER_TEST("Tier5", T5_PeakLimiter_StrictCeilingAndOverloadStress, "Peak Limiter Strict Ceiling & Overload Stress", test_T5_Challenger_PeakLimiter_StrictCeilingAndOverloadStress);
REGISTER_TEST("Tier5", T5_TelemetryRingBuffer_SaturationStress, "Telemetry Ring Buffer SPSC Saturation Stress", test_T5_Challenger_TelemetryRingBuffer_SaturationStress);
REGISTER_TEST("Tier5", T5_DecayToSilence_ZeroDenormals, "Decay to Silence Zero Denormals Verification", test_T5_Challenger_DecayToSilence_ZeroDenormals);
REGISTER_TEST("Tier5", T5_PresetCatalogFidelity_All10Presets, "Factory Preset Catalog 10/10 Fidelity Audit", test_T5_Challenger_PresetCatalogFidelity_All10Presets);

} // namespace ppf42::test
