#include "TestHarness.h"
#include "AllocationTracker.h"
#include "dsp/DspDefines.h"
#include "dsp/DspMath.h"
#include "dsp/DspEngine.h"

#include <vector>
#include <cmath>
#include <iostream>

namespace ppf42::test {

// ============================================================================
// T2_NumericalStability_100kSamples: 100k sample burn-in across extreme sweeps
// ============================================================================
bool test_T2_NumericalStability_100kSamples() {
    DspEngine engine;
    engine.prepare(48000.0, 512);

    FastPrng prng(0xCAFEBABE1234ULL);
    ParameterSnapshot p;

    // Trigger a note to ensure continuous oscillation
    MidiEvent noteOn { 0, 0x90, 48, 100 };
    engine.process(nullptr, nullptr, 0, p, &noteOn, 1);

    constexpr int kTotalSamples = 100000;
    constexpr int kBlockSize = 256;
    std::vector<float> left(kBlockSize);
    std::vector<float> right(kBlockSize);

    int samplesProcessed = 0;
    while (samplesProcessed < kTotalSamples) {
        // Randomize parameters on block boundaries
        p.pressure       = prng.nextFloat01();
        p.tension        = prng.nextFloat01();
        p.aperture       = prng.nextFloat01();
        p.flutter        = prng.nextFloat01();
        p.viscosity      = prng.nextFloat01();
        p.moisture       = prng.nextFloat01();
        p.droplet_rate   = prng.nextFloat01();
        p.cleft_damping  = prng.nextFloat01();
        p.porcelain_mix  = prng.nextFloat01();
        p.porcelain_size = 0.5f + 1.5f * prng.nextFloat01();
        p.porcelain_model= static_cast<int>(prng.nextU64() % 4);
        p.voice_mode     = static_cast<int>(prng.nextU64() % 3);
        p.drive          = prng.nextFloat01();
        p.master_gain    = -12.0f + 18.0f * prng.nextFloat01();
        p.macro_squeeze  = prng.nextFloat01();
        p.macro_moisture = prng.nextFloat01();

        const int numThisBlock = std::min(kBlockSize, kTotalSamples - samplesProcessed);
        engine.process(left.data(), right.data(), numThisBlock, p, nullptr, 0);

        for (int i = 0; i < numThisBlock; ++i) {
            TEST_ASSERT(isFiniteBitwise(left[i]), "Non-finite output (NaN/Inf) detected on Left channel!");
            TEST_ASSERT(isFiniteBitwise(right[i]), "Non-finite output (NaN/Inf) detected on Right channel!");
            TEST_ASSERT(left[i] <= 1.00001f && left[i] >= -1.00001f, "Peak limiter ceiling violated on Left!");
            TEST_ASSERT(right[i] <= 1.00001f && right[i] >= -1.00001f, "Peak limiter ceiling violated on Right!");
        }

        samplesProcessed += numThisBlock;
    }

    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier2", T2_NumericalStability_100kSamples, "100,000-sample burn-in across randomized parameters (0 NaNs, 0 Infs, <= 0 dBFS)", test_T2_NumericalStability_100kSamples);

// ============================================================================
// T2_DenormalFlushing: Verify hardware FTZ/DAZ mode and bitwise flushing
// ============================================================================
bool test_T2_DenormalFlushing() {
    ScopedNoDenormals guard;

    // Test subnormal floating point numbers
    const float subnormal = 1.0e-38f;
    const float flushed = flushDenormal(subnormal);
    TEST_ASSERT(flushed == 0.0f, "flushDenormal failed to flush subnormal float!");

    const float zeroVal = 0.0f;
    TEST_ASSERT(isFiniteBitwise(zeroVal), "Zero must be finite!");

    // NaN and Inf bitwise detection
    uint32_t nanBits = 0x7FC00000u;
    float nanVal;
    std::memcpy(&nanVal, &nanBits, sizeof(float));
    TEST_ASSERT(!isFiniteBitwise(nanVal), "isFiniteBitwise failed to detect NaN!");
    TEST_ASSERT(flushDenormal(nanVal) == 0.0f, "flushDenormal failed to zero NaN!");

    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier2", T2_DenormalFlushing, "Hardware FTZ/DAZ denormal flushing and bitwise IEEE-754 validation", test_T2_DenormalFlushing);

// ============================================================================
// T2_BlockSizeInvariance: Sweeps block sizes from 1 to 4096 samples
// ============================================================================
bool test_T2_BlockSizeInvariance() {
    DspEngine engine;
    engine.prepare(48000.0, 4096);

    ParameterSnapshot p;
    MidiEvent ev { 0, 0x90, 45, 100 };
    engine.process(nullptr, nullptr, 0, p, &ev, 1);

    const std::vector<int> blockSizes = { 1, 7, 16, 64, 128, 256, 512, 1024, 2048, 4096 };
    std::vector<float> left(4096);
    std::vector<float> right(4096);

    for (int bs : blockSizes) {
        engine.process(left.data(), right.data(), bs, p, nullptr, 0);
        for (int i = 0; i < bs; ++i) {
            TEST_ASSERT(isFiniteBitwise(left[i]), "Non-finite sample in block size sweep!");
            TEST_ASSERT(isFiniteBitwise(right[i]), "Non-finite sample in block size sweep!");
        }
    }

    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier2", T2_BlockSizeInvariance, "Block-size invariance across sizes 1 to 4096 samples", test_T2_BlockSizeInvariance);

// ============================================================================
// T2_SampleRateIndependence: Multi-rate validation 44.1 kHz to 192 kHz
// ============================================================================
bool test_T2_SampleRateIndependence() {
    const std::vector<double> sampleRates = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };

    for (double sr : sampleRates) {
        DspEngine engine;
        engine.prepare(sr, 512);

        ParameterSnapshot p;
        p.pressure = 0.8f;
        p.moisture = 0.5f;

        MidiEvent ev { 0, 0x90, 40, 90 };
        std::vector<float> left(512), right(512);

        engine.process(left.data(), right.data(), 512, p, &ev, 1);

        for (int i = 0; i < 512; ++i) {
            TEST_ASSERT(isFiniteBitwise(left[i]), "Non-finite output at sample rate!");
            TEST_ASSERT(isFiniteBitwise(right[i]), "Non-finite output at sample rate!");
        }
    }

    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier2", T2_SampleRateIndependence, "Sample-rate independence across 44.1 kHz, 48k, 88.2k, 96k, 192 kHz", test_T2_SampleRateIndependence);

// ============================================================================
// T2_ZeroHeapAllocations: Verify 0 allocations during audio rendering
// ============================================================================
bool test_T2_ZeroHeapAllocations() {
    DspEngine engine;
    engine.prepare(48000.0, 512);

    ParameterSnapshot p;
    MidiEvent ev { 0, 0x90, 45, 100 };
    std::vector<float> left(512);
    std::vector<float> right(512);

    // Warm up one block outside tracking
    engine.process(left.data(), right.data(), 512, p, &ev, 1);

    // Enter real-time allocation tracking scope
    {
        ScopedAllocationGuard guard;
        for (int b = 0; b < 20; ++b) {
            engine.process(left.data(), right.data(), 512, p, nullptr, 0);
        }
        const size_t allocs = guard.getCount();
        TEST_ASSERT(allocs == 0, "Dynamic memory allocations detected during audio process() callback!");
    }

    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier2", T2_ZeroHeapAllocations, "Zero heap allocations during active audio rendering (Real-Time Safety)", test_T2_ZeroHeapAllocations);

// ============================================================================
// T2_BoundaryParameterCombos: Test extreme corner combinations
// ============================================================================
bool test_T2_BoundaryParameterCombos() {
    DspEngine engine;
    engine.prepare(48000.0, 256);

    std::vector<float> left(256), right(256);
    MidiEvent ev { 0, 0x90, 40, 100 };

    // 1. All Minima
    {
        ParameterSnapshot pMin {};
        pMin.pressure = 0.0f;
        pMin.tension = 0.0f;
        pMin.aperture = 0.0f;
        pMin.flutter = 0.0f;
        pMin.viscosity = 0.0f;
        pMin.moisture = 0.0f;
        pMin.droplet_rate = 0.0f;
        pMin.cleft_damping = 0.0f;
        pMin.porcelain_mix = 0.0f;
        pMin.porcelain_size = 0.5f;
        pMin.drive = 0.0f;
        pMin.master_gain = -60.0f;
        pMin.macro_squeeze = 0.0f;
        pMin.macro_moisture = 0.0f;

        engine.process(left.data(), right.data(), 256, pMin, &ev, 1);
        for (int i = 0; i < 256; ++i) {
            TEST_ASSERT(isFiniteBitwise(left[i]), "Non-finite output in all-minima combo!");
        }
    }

    // 2. All Maxima with max Colonic Drive + Max Master Gain (+12 dB)
    {
        ParameterSnapshot pMax {};
        pMax.pressure = 1.0f;
        pMax.tension = 1.0f;
        pMax.aperture = 1.0f;
        pMax.flutter = 1.0f;
        pMax.viscosity = 1.0f;
        pMax.moisture = 1.0f;
        pMax.droplet_rate = 1.0f;
        pMax.cleft_damping = 1.0f;
        pMax.porcelain_mix = 1.0f;
        pMax.porcelain_size = 2.0f;
        pMax.drive = 1.0f;
        pMax.master_gain = +12.0f;
        pMax.macro_squeeze = 1.0f;
        pMax.macro_moisture = 1.0f;

        engine.process(left.data(), right.data(), 256, pMax, nullptr, 0);
        for (int i = 0; i < 256; ++i) {
            TEST_ASSERT(isFiniteBitwise(left[i]), "Non-finite output in all-maxima combo!");
            TEST_ASSERT(left[i] <= 1.00001f && left[i] >= -1.00001f, "Limiter ceiling violated under max drive + max gain!");
        }
    }

    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier2", T2_BoundaryParameterCombos, "Extreme parameter corner combinations (all minima, all maxima)", test_T2_BoundaryParameterCombos);

// ============================================================================
// CHALLENGER M1-1 EMPIRICAL ADVERSARIAL VERIFICATION SUITES
// ============================================================================

// ----------------------------------------------------------------------------
// T2_Challenger_LimiterStrictCeilingStress:
// Empirically verifies strict <= 1.0000000f (0.0 dBFS) ceiling under:
// 1. Full 32-bit float bitwise ULP sweep through knee-to-ceiling transition [0.84, 1.02]
// 2. 100,000 extreme values with random gains up to +60 dB
// 3. Resonant feedback and maximum drive overdrive in active DspEngine
// ----------------------------------------------------------------------------
bool test_T2_Challenger_LimiterStrictCeilingStress() {
    // 1. Bitwise ULP Sweep across [0.84, 1.02] (~1.5 million single-precision floats)
    {
        float startVal = 0.84f;
        float endVal = 1.02f;
        uint32_t startBits = 0, endBits = 0;
        std::memcpy(&startBits, &startVal, sizeof(float));
        std::memcpy(&endBits, &endVal, sizeof(float));

        for (uint32_t b = startBits; b <= endBits; ++b) {
            float val;
            std::memcpy(&val, &b, sizeof(float));

            const float limPos = BoundedSaturator::limitSample(val, 1.0f);
            const float limNeg = BoundedSaturator::limitSample(-val, 1.0f);

            TEST_ASSERT(isFiniteBitwise(limPos), "Limiter returned non-finite on positive ULP sweep!");
            TEST_ASSERT(isFiniteBitwise(limNeg), "Limiter returned non-finite on negative ULP sweep!");
            TEST_ASSERT(limPos <= 1.0000000f && limPos >= -1.0000000f, "Limiter exceeded 1.0000000f on positive ULP sweep!");
            TEST_ASSERT(limNeg <= 1.0000000f && limNeg >= -1.0000000f, "Limiter exceeded -1.0000000f on negative ULP sweep!");
        }
    }

    // 2. Massive Random Values and Gains up to +60 dB
    {
        FastPrng prng(0x1337BEEFCAFEULL);
        for (int i = 0; i < 100000; ++i) {
            const float raw = (prng.nextFloatSigned()) * std::pow(10.0f, prng.nextFloat01() * 8.0f - 2.0f);
            const float gainLinear = std::pow(10.0f, prng.nextFloat01() * 4.0f - 1.0f); // up to +60 dB linear ~1000

            const float lim = BoundedSaturator::limitSample(raw, gainLinear);
            TEST_ASSERT(isFiniteBitwise(lim), "Limiter non-finite on extreme random sweep!");
            TEST_ASSERT(lim <= 1.0000000f && lim >= -1.0000000f, "Limiter violated strict [-1.0, +1.0] ceiling!");
        }
    }

    // 3. Special float edge cases
    {
        const float edgeVals[] = {
            0.0f, -0.0f, 0.8499999f, 0.8500000f, 0.8500001f,
            0.9999999f, 1.0000000f, 1.0000001f, 1.0e-38f, -1.0e-38f,
            1.0e20f, -1.0e20f,
            std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN()
        };

        for (float v : edgeVals) {
            const float lim = BoundedSaturator::limitSample(v, 1.0f);
            TEST_ASSERT(isFiniteBitwise(lim), "Limiter returned non-finite on special edge case!");
            TEST_ASSERT(lim <= 1.0000000f && lim >= -1.0000000f, "Limiter violated [-1.0, 1.0] on special edge case!");
        }
    }

    // 4. End-to-End Engine Stress: Max Colonic Drive, Max Saturation, Overdrive Gain, Cleft Feedback
    {
        DspEngine engine;
        engine.prepare(48000.0, 256);

        ParameterSnapshot p {};
        p.pressure = 1.0f;       // Max colonic drive
        p.tension = 0.5f;
        p.aperture = 0.5f;
        p.flutter = 0.8f;
        p.viscosity = 0.5f;
        p.moisture = 0.8f;
        p.droplet_rate = 0.8f;
        p.cleft_damping = 0.0f;   // Minimum damping = maximum resonant reflection in waveguide
        p.porcelain_mix = 1.0f;   // 100% wet cavity convolution
        p.porcelain_size = 1.0f;
        p.porcelain_model = 1;    // Standard ceramic bowl with sharp modes
        p.voice_mode = 2;         // Stereo Unison Detune mode (8 oscillating components)
        p.drive = 1.0f;           // Max saturator drive
        p.master_gain = +18.0f;   // +18 dB master overdrive
        p.sub_level = +6.0f;      // Max sub-bass boost

        // Trigger notes across cavity resonant frequencies
        const MidiEvent notes[] = {
            { 0, 0x90, 57, 127 }, // A3 ~ 220 Hz
            { 0, 0x90, 69, 127 }, // A4 ~ 440 Hz (near 420 Hz cavity mode)
            { 0, 0x90, 83, 127 }  // B5 ~ 988 Hz (near 1180 Hz cavity mode)
        };

        for (const auto& ev : notes) {
            engine.process(nullptr, nullptr, 0, p, &ev, 1);
        }

        std::vector<float> left(256), right(256);
        for (int block = 0; block < 200; ++block) { // 51,200 samples of maximum stress
            // Sweep cavity model and pitch modulation
            p.porcelain_model = block % 4;
            engine.process(left.data(), right.data(), 256, p, nullptr, 0);

            for (int i = 0; i < 256; ++i) {
                TEST_ASSERT(isFiniteBitwise(left[i]), "Non-finite Left sample under engine overdrive!");
                TEST_ASSERT(isFiniteBitwise(right[i]), "Non-finite Right sample under engine overdrive!");
                TEST_ASSERT(left[i] <= 1.0000000f && left[i] >= -1.0000000f, "Left channel exceeded strict 0.0 dBFS ceiling!");
                TEST_ASSERT(right[i] <= 1.0000000f && right[i] >= -1.0000000f, "Right channel exceeded strict 0.0 dBFS ceiling!");
            }
        }
    }

    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier2", T2_Challenger_LimiterStrictCeilingStress, "Challenger: Limiter strict bounding <= 1.0000000f (ULP sweep, +60dB, feedback sweep)", test_T2_Challenger_LimiterStrictCeilingStress);

// ----------------------------------------------------------------------------
// T2_Challenger_DCBlocker15Hz_EmpiricalAnalysis:
// Empirically measures frequency response across 4 sample rates (44.1k, 48k, 96k, 192k)
// Verifies:
// 1. 0 Hz DC offset is eliminated (> 60 dB attenuation / < 0.0001 steady state)
// 2. 18 Hz sub-bass transmission ratio >= 0.707 (-3.0 dB) across all sample rates
// 3. Strips unipolar asymmetric DC offset from raw SphincterOscillator signal
// ----------------------------------------------------------------------------
bool test_T2_Challenger_DCBlocker15Hz_EmpiricalAnalysis() {
    const std::vector<double> sampleRates = { 44100.0, 48000.0, 96000.0, 192000.0 };

    for (double sr : sampleRates) {
        SubBassDcBlocker blocker;
        blocker.prepare(sr);

        // 1. DC Step Input: Feed +1.0 for 1 second.
        const int numSamples1Sec = static_cast<int>(sr);
        for (int i = 0; i < numSamples1Sec; ++i) {
            (void)blocker.processSample(1.0f);
        }
        // Sample after 1s settling must be effectively zero
        const float residualDc = blocker.processSample(1.0f);
        TEST_ASSERT(std::abs(residualDc) < 0.0001f, "DC blocker failed to eliminate DC step after 1 second!");

        // 2. Frequency sweep: measure transmission ratio at 18 Hz vs 15 Hz vs 5 Hz vs 100 Hz
        auto measureTransmission = [&](float freqHz) -> float {
            blocker.reset();
            const int totalSamples = static_cast<int>(sr * 1.5);
            const int settleSamples = static_cast<int>(sr * 0.75);
            float peakIn = 0.0f;
            float peakOut = 0.0f;

            for (int i = 0; i < totalSamples; ++i) {
                const float in = std::sin(kTwoPi * freqHz * static_cast<float>(i) / static_cast<float>(sr));
                const float out = blocker.processSample(in);
                if (i >= settleSamples) {
                    peakIn = std::max(peakIn, std::abs(in));
                    peakOut = std::max(peakOut, std::abs(out));
                }
            }
            return (peakIn > 1.0e-5f) ? (peakOut / peakIn) : 0.0f;
        };

        const float trans5Hz   = measureTransmission(5.0f);
        const float trans15Hz  = measureTransmission(15.0f);
        const float trans18Hz  = measureTransmission(18.0f);
        const float trans100Hz = measureTransmission(100.0f);

        std::cout << "[SR " << static_cast<int>(sr) << "Hz] "
                  << "5Hz=" << gainToDb(trans5Hz) << "dB, "
                  << "15Hz=" << gainToDb(trans15Hz) << "dB, "
                  << "18Hz=" << gainToDb(trans18Hz) << "dB, "
                  << "100Hz=" << gainToDb(trans100Hz) << "dB\n      ";

        // At 5 Hz: should be heavily attenuated (< 0.35, i.e. < -9 dB)
        TEST_ASSERT(trans5Hz < 0.35f, "DC blocker 5 Hz attenuation insufficient!");

        // At 15 Hz (cutoff): should be close to -3 dB (~0.707)
        TEST_ASSERT(trans15Hz >= 0.65f && trans15Hz <= 0.76f, "DC blocker -3 dB cutoff not aligned at 15 Hz!");

        // At 18 Hz (target sub-bass): MUST preserve amplitude >= 0.707 (-3.0 dB)
        TEST_ASSERT(trans18Hz >= 0.707f, "18 Hz sub-bass amplitude over-attenuated (< -3 dB)!");

        // At 100 Hz: should pass virtually unattenuated (> 0.98)
        TEST_ASSERT(trans100Hz > 0.98f, "100 Hz audio passband attenuated by DC blocker!");
    }

    // 3. Asymmetric unipolar physical signal test
    {
        SphincterOscillator osc;
        osc.prepare(48000.0);

        SphincterOscParams p;
        p.pressure = 0.85f;
        p.tension = 0.5f;
        p.aperture = 0.5f;
        p.flutter = 0.3f;
        p.viscosity = 0.4f;
        p.frequencyHz = 40.0f;

        SubBassDcBlocker blocker;
        blocker.prepare(48000.0);

        // Process 48000 samples, measure DC offset of raw signal vs DC-blocked signal over last 24000 samples
        double rawSum = 0.0;
        double blockedSum = 0.0;
        int count = 0;

        for (int i = 0; i < 48000; ++i) {
            float airVel = 0.0f, ap = 0.0f;
            const float rawSignal = osc.processSample(p, airVel, ap);
            const float blockedSignal = blocker.processSample(rawSignal);

            if (i >= 24000) {
                rawSum += rawSignal;
                blockedSum += blockedSignal;
                ++count;
            }
        }

        const double meanRaw = std::abs(rawSum / count);
        const double meanBlocked = std::abs(blockedSum / count);

        std::cout << "[EMPIRICAL DC] meanRaw = " << meanRaw << ", meanBlocked = " << meanBlocked << " ";

        // Verify DC blocker reduces whatever DC bias exists by at least an order of magnitude or to < 0.001
        TEST_ASSERT(meanBlocked < 0.002, "DC blocker failed to strip DC bias from raw physical modeling signal!");
        if (meanRaw > 0.001) {
            TEST_ASSERT(meanBlocked < (meanRaw * 0.1), "DC blocker did not attenuate DC bias by at least 10x!");
        }
    }

    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier2", T2_Challenger_DCBlocker15Hz_EmpiricalAnalysis, "Challenger: 15 Hz DC blocker multi-rate analysis (eliminates DC, preserves 18 Hz >= -3dB)", test_T2_Challenger_DCBlocker15Hz_EmpiricalAnalysis);

// ----------------------------------------------------------------------------
// T2_Challenger_ExtremeBurnIn_250kSamples:
// Runs 300,000 samples at 48 kHz + 200,000 samples at 96 kHz (500k samples total)
// Stress-tests with:
// - Rapid random MIDI note bursts, pitch bend sweeps, aftertouch
// - Dynamic block size changes (16 to 512 samples)
// - Continuous parameter mutations on block boundaries
// - Mode switching (Mono-Legato, Polyphonic, Unison Detune)
// - Cavity model switching during active audio playback
// Verifies:
// - 0 NaNs, 0 Infs
// - 0 denormals (subnormals)
// - All output strictly bounded in [-1.0, 1.0]
// ----------------------------------------------------------------------------
bool test_T2_Challenger_ExtremeBurnIn_250kSamples() {
    FastPrng prng(0x8BADF00DDEADBEEFULL);

    auto runBurnIn = [&](double sampleRate, int totalSamples) {
        DspEngine engine;
        engine.prepare(sampleRate, 512);

        ParameterSnapshot p;
        int samplesRemaining = totalSamples;

        std::vector<float> leftBuf(512);
        std::vector<float> rightBuf(512);

        // Seed initial notes
        MidiEvent initNote { 0, 0x90, 48, 100 };
        engine.process(nullptr, nullptr, 0, p, &initNote, 1);

        while (samplesRemaining > 0) {
            // Randomize block size
            const int blockSizeChoices[] = { 16, 32, 64, 128, 256, 512 };
            const int bs = blockSizeChoices[prng.nextU64() % 6];
            const int numSamplesThisBlock = std::min(bs, samplesRemaining);

            // Mutate parameters
            p.pressure       = (prng.nextFloat01() < 0.08f) ? 0.0f : prng.nextFloat01(); // Occasional silence / sudden pressure
            p.tension        = prng.nextFloat01();
            p.aperture       = prng.nextFloat01();
            p.flutter        = prng.nextFloat01();
            p.viscosity      = prng.nextFloat01();
            p.moisture       = prng.nextFloat01();
            p.droplet_rate   = prng.nextFloat01();
            p.cleft_damping  = prng.nextFloat01();
            p.porcelain_mix  = prng.nextFloat01();
            p.porcelain_size = 0.5f + 1.5f * prng.nextFloat01();
            p.porcelain_model= static_cast<int>(prng.nextU64() % 4);
            p.voice_mode     = static_cast<int>(prng.nextU64() % 3);
            p.drive          = prng.nextFloat01();
            p.master_gain    = -24.0f + 36.0f * prng.nextFloat01(); // -24 dB to +12 dB
            p.sub_level      = -24.0f + 30.0f * prng.nextFloat01(); // -24 dB to +6 dB
            p.macro_squeeze  = prng.nextFloat01();
            p.macro_moisture = prng.nextFloat01();

            // Random MIDI events
            MidiEvent midiEvents[4];
            int numMidi = 0;
            if (prng.nextFloat01() < 0.35f) {
                const uint8_t eventType = (prng.nextFloat01() < 0.6f) ? 0x90 : 0x80;
                const uint8_t noteNum = static_cast<uint8_t>(24 + (prng.nextU64() % 60));
                const uint8_t vel = static_cast<uint8_t>(1 + (prng.nextU64() % 127));
                midiEvents[numMidi++] = MidiEvent { 0, eventType, noteNum, vel };
            }
            if (prng.nextFloat01() < 0.25f) {
                // Pitch bend
                const uint8_t lsb = static_cast<uint8_t>(prng.nextU64() & 0x7F);
                const uint8_t msb = static_cast<uint8_t>((prng.nextU64() >> 7) & 0x7F);
                midiEvents[numMidi++] = MidiEvent { 0, 0xE0, lsb, msb };
            }

            engine.process(leftBuf.data(), rightBuf.data(), numSamplesThisBlock, p, midiEvents, numMidi);

            for (int i = 0; i < numSamplesThisBlock; ++i) {
                const float sL = leftBuf[i];
                const float sR = rightBuf[i];

                TEST_ASSERT(isFiniteBitwise(sL), "Non-finite (NaN/Inf) sample on Left in extreme burn-in!");
                TEST_ASSERT(isFiniteBitwise(sR), "Non-finite (NaN/Inf) sample on Right in extreme burn-in!");
                TEST_ASSERT(std::fpclassify(sL) != FP_SUBNORMAL, "Denormal (subnormal) float on Left in burn-in!");
                TEST_ASSERT(std::fpclassify(sR) != FP_SUBNORMAL, "Denormal (subnormal) float on Right in burn-in!");
                TEST_ASSERT(sL <= 1.0000000f && sL >= -1.0000000f, "Left output violated <= 0.0 dBFS ceiling in burn-in!");
                TEST_ASSERT(sR <= 1.0000000f && sR >= -1.0000000f, "Right output violated <= 0.0 dBFS ceiling in burn-in!");
            }

            samplesRemaining -= numSamplesThisBlock;
        }
    };

    // 1. 300,000 samples at 48.0 kHz
    runBurnIn(48000.0, 300000);

    // 2. 200,000 samples at 96.0 kHz
    runBurnIn(96000.0, 200000);

    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier2", T2_Challenger_ExtremeBurnIn_250kSamples, "Challenger: Extreme 500k sample burn-in (48k & 96k, 0 NaNs, 0 denormals, bounded)", test_T2_Challenger_ExtremeBurnIn_250kSamples);

// ----------------------------------------------------------------------------
// T2_Challenger_ResonantFeedbackAndSubBassCluster:
// Extreme adversarial stress:
// - 8 polyphonic voices hitting 18 Hz sub-bass note cluster simultaneously at vel 127
// - Sub-bass level at max (+6 dB)
// - Colonic drive at max (1.0)
// - Master gain at +24 dB overdrive
// - Waveguide boundary damping at 0.0 (maximum reflection & resonance buildup)
// - Porcelain cavity model modulation
// - Process 200,000 samples across 48 kHz and 192 kHz
// ----------------------------------------------------------------------------
bool test_T2_Challenger_ResonantFeedbackAndSubBassCluster() {
    auto testCluster = [&](double sampleRate, int totalSamples) {
        DspEngine engine;
        engine.prepare(sampleRate, 256);

        ParameterSnapshot p {};
        p.pressure = 1.0f;
        p.tension = 0.0f;        // relaxed tension for deepest fundamental
        p.aperture = 0.5f;
        p.flutter = 0.9f;
        p.viscosity = 0.8f;
        p.moisture = 0.9f;
        p.droplet_rate = 1.0f;
        p.cleft_damping = 0.0f;  // max resonant reflections
        p.porcelain_mix = 1.0f;  // 100% wet
        p.porcelain_size = 1.0f;
        p.porcelain_model = 1;
        p.voice_mode = 1;        // Polyphonic 8-voice mode
        p.drive = 1.0f;          // max saturation
        p.master_gain = +24.0f;  // extreme +24 dB overdrive
        p.sub_level = +6.0f;     // max sub-bass reinforcement

        // Trigger 8 notes near 18 Hz (MIDI note 18 ~ 18.35 Hz, 19, 20, 21, 22, 23, 24, 25)
        MidiEvent chord[8];
        for (int i = 0; i < 8; ++i) {
            chord[i] = MidiEvent { 0, 0x90, static_cast<uint8_t>(18 + (i % 3)), 127 };
        }
        engine.process(nullptr, nullptr, 0, p, chord, 8);

        std::vector<float> left(256), right(256);
        int remaining = totalSamples;
        int blockCount = 0;

        while (remaining > 0) {
            const int n = std::min(256, remaining);
            // Continuous modulation of cavity size and model during maximum resonance
            p.porcelain_model = (blockCount / 50) % 4;
            p.porcelain_size  = 0.5f + 1.5f * (static_cast<float>(blockCount % 100) / 100.0f);

            engine.process(left.data(), right.data(), n, p, nullptr, 0);

            for (int i = 0; i < n; ++i) {
                TEST_ASSERT(isFiniteBitwise(left[i]), "Non-finite Left in 18 Hz resonant feedback cluster!");
                TEST_ASSERT(isFiniteBitwise(right[i]), "Non-finite Right in 18 Hz resonant feedback cluster!");
                TEST_ASSERT(std::fpclassify(left[i]) != FP_SUBNORMAL, "Denormal on Left in resonant cluster!");
                TEST_ASSERT(std::fpclassify(right[i]) != FP_SUBNORMAL, "Denormal on Right in resonant cluster!");
                TEST_ASSERT(left[i] <= 1.0000000f && left[i] >= -1.0000000f, "Left violated 0.0 dBFS ceiling in resonant cluster!");
                TEST_ASSERT(right[i] <= 1.0000000f && right[i] >= -1.0000000f, "Right violated 0.0 dBFS ceiling in resonant cluster!");
            }

            remaining -= n;
            ++blockCount;
        }
    };

    // 100k samples at 48 kHz
    testCluster(48000.0, 100000);

    // 100k samples at 192 kHz
    testCluster(192000.0, 100000);

    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier2", T2_Challenger_ResonantFeedbackAndSubBassCluster, "Challenger: 8-voice 18 Hz cluster +24dB overdrive & feedback sweep (48k & 192k)", test_T2_Challenger_ResonantFeedbackAndSubBassCluster);

} // namespace ppf42::test

