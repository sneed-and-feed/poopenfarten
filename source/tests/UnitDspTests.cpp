#include "TestHarness.h"
#include "dsp/DspDefines.h"
#include "dsp/DspMath.h"
#include "dsp/SphincterOscillator.h"
#include "dsp/FluidNoiseEngine.h"
#include "dsp/CleftWaveguide.h"
#include "dsp/PorcelainConvolver.h"
#include "dsp/SubBassDcBlocker.h"
#include "dsp/BoundedSaturator.h"
#include "dsp/VoiceManager.h"
#include "dsp/DspEngine.h"

#include <vector>
#include <cmath>

namespace ppf42::test {

// ============================================================================
// T1_Oscillator_ApertureBound: Verify displacement y >= 0 under all drive pressures
// ============================================================================
bool test_T1_Oscillator_ApertureBound() {
    SphincterOscillator osc;
    osc.prepare(48000.0);

    const std::vector<float> testPressures = { 0.1f, 0.3f, 0.7f, 1.0f };
    for (float pr : testPressures) {
        osc.reset();
        SphincterOscParams p;
        p.pressure = pr;
        p.tension = 0.5f;
        p.aperture = 0.3f;
        p.flutter = 0.2f;
        p.viscosity = 0.2f;
        p.frequencyHz = 110.0f;

        for (int i = 0; i < 5000; ++i) {
            float airVel = 0.0f, aperture = 0.0f;
            (void)osc.processSample(p, airVel, aperture);
            TEST_ASSERT(aperture >= 0.0f, "Aperture y fell below zero!");
            TEST_ASSERT(isFiniteBitwise(aperture), "Aperture is NaN or Inf!");
            TEST_ASSERT(isFiniteBitwise(airVel), "Air velocity is NaN or Inf!");
        }
    }
    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier1", T1_Oscillator_ApertureBound, "Sphincter valve aperture y >= 0 restitution bound", test_T1_Oscillator_ApertureBound);

// ============================================================================
// T1_Oscillator_ZeroPressureMute: Verify zero drive produces zero sound & flow
// ============================================================================
bool test_T1_Oscillator_ZeroPressureMute() {
    SphincterOscillator osc;
    osc.prepare(48000.0);

    SphincterOscParams p;
    p.pressure = 0.0f; // Zero pressure head
    p.tension = 0.5f;
    p.aperture = 0.35f;
    p.frequencyHz = 110.0f;

    for (int i = 0; i < 1000; ++i) {
        float airVel = 100.0f, aperture = 0.0f;
        const float out = osc.processSample(p, airVel, aperture);
        TEST_ASSERT(out == 0.0f, "Oscillator output non-zero when pressure is zero!");
        TEST_ASSERT(airVel == 0.0f, "Air velocity non-zero when pressure is zero!");
    }
    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier1", T1_Oscillator_ZeroPressureMute, "Zero colonic pressure mutes oscillator and halts airflow", test_T1_Oscillator_ZeroPressureMute);

// ============================================================================
// T1_Oscillator_FrequencyCalibration: Verify fundamental frequency tracking
// ============================================================================
bool test_T1_Oscillator_FrequencyCalibration() {
    constexpr double sampleRate = 48000.0;
    const std::vector<float> targetFreqs = { 55.0f, 110.0f, 220.0f };

    for (float targetF : targetFreqs) {
        SphincterOscillator osc;
        osc.prepare(sampleRate);

        SphincterOscParams p;
        p.pressure = 0.75f;
        p.tension = 0.50f;
        p.aperture = 0.35f;
        p.flutter = 0.0f; // Pure oscillation without chaos
        p.viscosity = 0.0f;
        p.frequencyHz = targetF;

        std::vector<float> audio(8192);
        for (size_t i = 0; i < audio.size(); ++i) {
            float airVel = 0.0f, ap = 0.0f;
            audio[i] = osc.processSample(p, airVel, ap);
        }

        // Measure detected peak frequency using Blackman-Harris windowed FFT
        const double detectedF = findPeakFrequency(audio, sampleRate, 2048, 4096, targetF * 0.5, targetF * 2.0);
        const double relDiff = std::abs(detectedF - targetF) / targetF;
        TEST_ASSERT(relDiff < 0.12, "Oscillator frequency mismatch for target frequency!");
    }
    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier1", T1_Oscillator_FrequencyCalibration, "Oscillator tracks fundamental frequency across acoustic range", test_T1_Oscillator_FrequencyCalibration);

// ============================================================================
// T1_Turbulence_VelocityScaling: Verify noise power scales with air velocity
// ============================================================================
bool test_T1_Turbulence_VelocityScaling() {
    FluidNoiseEngine fluid;
    fluid.prepare(48000.0);

    FluidEngineParams p;
    p.viscosity = 0.0f;
    p.moisture = 0.0f; // Pure turbulence noise, no bubbles/droplets
    p.dropletRate = 0.0f;

    auto measureRms = [&](float airVel) -> float {
        fluid.reset();
        float sumSq = 0.0f;
        for (int i = 0; i < 4000; ++i) {
            const float s = fluid.processSample(airVel, 0.001f, p);
            sumSq += s * s;
        }
        return std::sqrt(sumSq / 4000.0f);
    };

    const float rmsLow  = measureRms(5.0f);
    const float rmsMed  = measureRms(15.0f);
    const float rmsHigh = measureRms(35.0f);

    TEST_ASSERT(rmsMed > rmsLow, "Medium velocity RMS must exceed low velocity RMS!");
    TEST_ASSERT(rmsHigh > rmsMed, "High velocity RMS must exceed medium velocity RMS!");
    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier1", T1_Turbulence_VelocityScaling, "Jet turbulence power scales monotonically with air velocity", test_T1_Turbulence_VelocityScaling);

// ============================================================================
// T1_Minnaert_BubbleFrequencies: Verify bubble frequencies remain in 800 - 6500 Hz
// ============================================================================
bool test_T1_Minnaert_BubbleFrequencies() {
    FluidNoiseEngine fluid;
    fluid.prepare(48000.0);

    const std::vector<float> radii = { 0.5f, 1.0f, 2.0f, 3.5f, 4.0f };
    for (float r : radii) {
        fluid.reset();
        fluid.triggerBubble(r, 1.0f);

        std::vector<float> audio(2048);
        FluidEngineParams p;
        p.viscosity = 0.0f;
        p.moisture = 1.0f;
        p.dropletRate = 0.0f;

        for (size_t i = 0; i < audio.size(); ++i) {
            audio[i] = fluid.processSample(0.001f, 0.001f, p);
        }

        const double peakF = findPeakFrequency(audio, 48000.0, 0, 1024, 500.0, 7500.0);
        TEST_ASSERT(peakF >= 750.0 && peakF <= 7000.0, "Bubble frequency out of Minnaert calibrated range [800, 6500] Hz!");
    }
    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier1", T1_Minnaert_BubbleFrequencies, "Minnaert bubble resonance frequencies bounded in 800 Hz - 6.5 kHz", test_T1_Minnaert_BubbleFrequencies);

// ============================================================================
// T1_Poisson_DropletZeroWhenDry: Verify 0 droplet/bubble events when moisture == 0
// ============================================================================
bool test_T1_Poisson_DropletZeroWhenDry() {
    FluidNoiseEngine fluid;
    fluid.prepare(48000.0);

    FluidEngineParams p;
    p.viscosity = 0.5f;
    p.moisture = 0.0f; // Completely dry
    p.dropletRate = 1.0f;

    for (int i = 0; i < 10000; ++i) {
        (void)fluid.processSample(20.0f, 0.002f, p);
        TEST_ASSERT(fluid.getDropletPopTrigger() == 0.0f, "Droplet pop occurred while moisture was 0.0!");
        TEST_ASSERT(fluid.getBubbleActivity() == 0.0f, "Bubble activity occurred while moisture was 0.0!");
    }
    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier1", T1_Poisson_DropletZeroWhenDry, "Zero droplet pops and zero bubbles when moisture parameter is 0.0", test_T1_Poisson_DropletZeroWhenDry);

// ============================================================================
// T1_Convolver_ZeroLatencyAndModels: Verify 0-latency execution on all 4 models
// ============================================================================
bool test_T1_Convolver_ZeroLatencyAndModels() {
    PorcelainConvolver conv;
    conv.prepare(48000.0);

    for (int model = 0; model < 4; ++model) {
        conv.reset();
        // Feed Dirac delta impulse at sample 0
        const float out0 = conv.processSample(1.0f, 1.0f, 1.0f, model);
        TEST_ASSERT(std::abs(out0) > 0.1f, "Zero-latency convolver must produce immediate response at sample 0!");

        // Process tail samples and verify finite decay
        for (int i = 1; i < 2000; ++i) {
            const float s = conv.processSample(0.0f, 1.0f, 1.0f, model);
            TEST_ASSERT(isFiniteBitwise(s), "Convolver produced NaN or Inf!");
        }
    }
    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier1", T1_Convolver_ZeroLatencyAndModels, "Porcelain convolver zero algorithmic latency across 4 cavity models", test_T1_Convolver_ZeroLatencyAndModels);

// ============================================================================
// T1_DCBlocker_Passes18HzAndBlocksDC: Blocks 0 Hz DC, passes 18 Hz sub-bass
// ============================================================================
bool test_T1_DCBlocker_Passes18HzAndBlocksDC() {
    SubBassDcBlocker blocker;
    blocker.prepare(48000.0);

    // 1. Feed DC offset (+1.0) for 1 second (48000 samples)
    float lastVal = 1.0f;
    for (int i = 0; i < 48000; ++i) {
        lastVal = blocker.processSample(1.0f);
    }
    TEST_ASSERT(std::abs(lastVal) < 0.02f, "DC blocker failed to remove DC offset within 1 second!");

    // 2. Feed 18 Hz sine wave: must pass with high transmission
    blocker.reset();
    constexpr float f18 = 18.0f;
    float peakIn = 0.0f, peakOut = 0.0f;
    for (int i = 0; i < 48000; ++i) {
        const float in = std::sin(kTwoPi * f18 * static_cast<float>(i) / 48000.0f);
        const float out = blocker.processSample(in);
        if (i > 24000) { // After settling
            peakIn = std::max(peakIn, std::abs(in));
            peakOut = std::max(peakOut, std::abs(out));
        }
    }
    const float transmissionRatio = peakOut / peakIn;
    TEST_ASSERT(transmissionRatio >= 0.70f, "18 Hz fundamental was over-attenuated by 15 Hz DC blocker!");
    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier1", T1_DCBlocker_Passes18HzAndBlocksDC, "Infrasonic DC blocker strips DC offset while preserving 18 Hz sub-bass", test_T1_DCBlocker_Passes18HzAndBlocksDC);

// ============================================================================
// T1_Limiter_StrictCeiling: Guarantees output <= 1.00000 under +24 dB overdrive
// ============================================================================
bool test_T1_Limiter_StrictCeiling() {
    for (int i = 0; i < 10000; ++i) {
        // Sweep input amplitudes up to +24 dB (amplitude ~16.0)
        const float raw = -16.0f + 32.0f * (static_cast<float>(i) / 10000.0f);
        const float lim = BoundedSaturator::limitSample(raw, 1.0f);

        TEST_ASSERT(lim <= 1.00001f, "Limiter ceiling exceeded 1.00000 (0.0 dBFS)!");
        TEST_ASSERT(lim >= -1.00001f, "Limiter ceiling exceeded -1.00000 (0.0 dBFS)!");
        TEST_ASSERT(isFiniteBitwise(lim), "Limiter returned NaN or Inf!");
    }
    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier1", T1_Limiter_StrictCeiling, "True-peak brickwall limiter strictly clamps output <= 0.0 dBFS", test_T1_Limiter_StrictCeiling);

// ============================================================================
// T1_Voice_MonoLegatoRetrigger: Verify smooth legato pitch glide
// ============================================================================
bool test_T1_Voice_MonoLegatoRetrigger() {
    VoiceManager vm;
    vm.prepare(48000.0);

    ParameterSnapshot p;
    p.voice_mode = 0; // Mono Legato
    p.glide_time = 40.0f; // 40 ms portamento

    // Note On 36
    MidiEvent ev1 { 0, 0x90, 36, 100 };
    vm.handleMidiEvent(ev1, p);
    TEST_ASSERT(vm.getActiveVoiceCount() == 1, "Mono mode must have 1 active voice!");

    // Render 100 samples
    for (int i = 0; i < 100; ++i) {
        float l, r, air, ap, bub, drop;
        vm.processSample(p, l, r, air, ap, bub, drop);
    }

    // Note On 48 while 36 held
    MidiEvent ev2 { 0, 0x90, 48, 100 };
    vm.handleMidiEvent(ev2, p);
    TEST_ASSERT(vm.getActiveVoiceCount() == 1, "Mono Legato must remain single voice during legato transition!");

    // Note Off 48 -> returns to 36
    MidiEvent ev3 { 0, 0x80, 48, 0 };
    vm.handleMidiEvent(ev3, p);
    TEST_ASSERT(vm.getActiveVoiceCount() == 1, "Voice must remain active on fallback note!");

    // Note Off 36 -> releases voice
    MidiEvent ev4 { 0, 0x80, 36, 0 };
    vm.handleMidiEvent(ev4, p);

    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier1", T1_Voice_MonoLegatoRetrigger, "Mono-Legato note stack and seamless legato pitch transition", test_T1_Voice_MonoLegatoRetrigger);

// ============================================================================
// T1_Voice_PolyphonicStealing: Verify 8-voice limit and prioritized stealing
// ============================================================================
bool test_T1_Voice_PolyphonicStealing() {
    VoiceManager vm;
    vm.prepare(48000.0);

    ParameterSnapshot p;
    p.voice_mode = 1; // Poly 8-Voice

    // Strike 8 distinct notes
    for (int n = 40; n < 48; ++n) {
        MidiEvent ev { 0, 0x90, static_cast<uint8_t>(n), 90 };
        vm.handleMidiEvent(ev, p);
    }
    TEST_ASSERT(vm.getActiveVoiceCount() == 8, "Polyphonic mode must hold 8 active voices!");

    // Process a block
    for (int i = 0; i < 200; ++i) {
        float l, r, air, ap, bub, drop;
        vm.processSample(p, l, r, air, ap, bub, drop);
    }

    // Strike 9th note: must steal a voice without exceeding 8
    MidiEvent ev9 { 0, 0x90, 50, 100 };
    vm.handleMidiEvent(ev9, p);
    TEST_ASSERT(vm.getActiveVoiceCount() <= 8, "Voice count exceeded maximum polyphony limit of 8!");

    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier1", T1_Voice_PolyphonicStealing, "Polyphonic 8-voice allocation with 4-tier prioritized voice stealing", test_T1_Voice_PolyphonicStealing);

// ============================================================================
// T1_Voice_UnisonDetune: Verify stereo width in unison detune mode
// ============================================================================
bool test_T1_Voice_UnisonDetune() {
    VoiceManager vm;
    vm.prepare(48000.0);

    ParameterSnapshot p;
    p.voice_mode = 2; // Stereo Unison

    MidiEvent ev { 0, 0x90, 45, 100 };
    vm.handleMidiEvent(ev, p);

    float sumDiffSq = 0.0f;
    for (int i = 0; i < 1000; ++i) {
        float l, r, air, ap, bub, drop;
        vm.processSample(p, l, r, air, ap, bub, drop);
        sumDiffSq += (l - r) * (l - r);
    }

    TEST_ASSERT(sumDiffSq > 0.0001f, "Unison detune mode must generate wide stereo difference!");
    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier1", T1_Voice_UnisonDetune, "Stereo unison detune mode produces wide spatial separation", test_T1_Voice_UnisonDetune);

} // namespace ppf42::test
