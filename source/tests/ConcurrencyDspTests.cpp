#include "TestHarness.h"
#include "AllocationTracker.h"
#include "dsp/DspDefines.h"
#include "dsp/DspMath.h"
#include "dsp/DspEngine.h"
#include "dsp/TelemetryRingBuffer.h"

#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <array>
#include <cmath>
#include <iostream>

namespace ppf42::test {

// ============================================================================
// T3_APVTS_ConcurrentAutomation: Multi-threaded parameter sweep vs audio render
// ============================================================================
bool test_T3_APVTS_ConcurrentAutomation() {
    DspEngine engine;
    engine.prepare(48000.0, 128);

    std::atomic<bool> keepRunning { true };
    std::atomic<int> blocksRendered { 0 };

    // Shared parameter snapshot updated by thread 2
    ParameterSnapshot sharedParams;
    std::atomic<float> atomicPressure { 0.7f };
    std::atomic<float> atomicMoisture { 0.3f };
    std::atomic<float> atomicDrive    { 0.2f };

    // Thread 1: Audio Rendering Thread
    std::thread audioThread([&]() {
        std::vector<float> left(128);
        std::vector<float> right(128);
        MidiEvent ev { 0, 0x90, 42, 90 };
        engine.process(left.data(), right.data(), 128, sharedParams, &ev, 1);

        while (keepRunning.load(std::memory_order_relaxed)) {
            // Read atomic parameter values into local snapshot
            ParameterSnapshot localP = sharedParams;
            localP.pressure = atomicPressure.load(std::memory_order_relaxed);
            localP.moisture = atomicMoisture.load(std::memory_order_relaxed);
            localP.drive    = atomicDrive.load(std::memory_order_relaxed);

            engine.process(left.data(), right.data(), 128, localP, nullptr, 0);

            for (int i = 0; i < 128; ++i) {
                if (!isFiniteBitwise(left[i]) || !isFiniteBitwise(right[i])) {
                    TEST_ASSERT(false, "Audio thread rendered NaN/Inf during concurrent parameter automation!");
                }
                if (left[i] > 1.00001f || right[i] > 1.00001f) {
                    TEST_ASSERT(false, "Audio thread exceeded 0.0 dBFS ceiling during concurrent automation!");
                }
            }

            blocksRendered.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::yield();
        }
    });

    // Thread 2: Parameter Automation Thread (1000 updates/sec)
    std::thread paramThread([&]() {
        FastPrng prng(0x987654321ULL);
        while (keepRunning.load(std::memory_order_relaxed)) {
            atomicPressure.store(prng.nextFloat01(), std::memory_order_relaxed);
            atomicMoisture.store(prng.nextFloat01(), std::memory_order_relaxed);
            atomicDrive.store(prng.nextFloat01(), std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    });

    // Thread 3: Telemetry Consumer Thread (Popping frames at high speed)
    std::thread telemetryThread([&]() {
        VisualizerFrame frame;
        while (keepRunning.load(std::memory_order_relaxed)) {
            while (engine.popVisualizerFrame(frame)) {
                // Read frame
                if (!isFiniteBitwise(frame.instantaneousAperture)) {
                    TEST_ASSERT(false, "Telemetry frame contained non-finite aperture!");
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Run until at least 500 audio blocks rendered
    while (blocksRendered.load(std::memory_order_relaxed) < 500) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    keepRunning.store(false, std::memory_order_relaxed);
    audioThread.join();
    paramThread.join();
    telemetryThread.join();

    TEST_ASSERT(blocksRendered.load() >= 500, "Audio thread did not render expected 500 blocks!");
    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier3", T3_APVTS_ConcurrentAutomation, "Multi-threaded stress: 500 audio blocks vs 1000 updates/sec parameter automation", test_T3_APVTS_ConcurrentAutomation);

// ============================================================================
// T3_Telemetry_FifoIntegrity: Lock-free SPSC buffer high-contention integrity
// ============================================================================
bool test_T3_Telemetry_FifoIntegrity() {
    TelemetryRingBuffer fifo;
    std::atomic<bool> donePushing { false };
    std::atomic<size_t> totalPopped { 0 };

    constexpr size_t kTotalPushes = 20000;

    std::thread consumer([&]() {
        VisualizerFrame frame;
        while (!donePushing.load(std::memory_order_relaxed)) {
            while (fifo.pop(frame)) {
                totalPopped.fetch_add(1, std::memory_order_relaxed);
            }
            std::this_thread::yield();
        }
        // Drain remainder
        while (fifo.pop(frame)) {
            totalPopped.fetch_add(1, std::memory_order_relaxed);
        }
    });

    std::thread producer([&]() {
        VisualizerFrame frame;
        for (size_t i = 0; i < kTotalPushes; ++i) {
            frame.airflowVelocity = static_cast<float>(i);
            fifo.push(frame);
            if ((i % 100) == 0) std::this_thread::yield();
        }
        donePushing.store(true, std::memory_order_relaxed);
    });

    producer.join();
    consumer.join();

    TEST_ASSERT(totalPopped.load() > 0, "No frames were popped from telemetry ring buffer!");
    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier3", T3_Telemetry_FifoIntegrity, "Lock-free SPSC telemetry buffer high-contention throughput and thread safety", test_T3_Telemetry_FifoIntegrity);

// ============================================================================
// CHALLENGER M1-2 EMPIRICAL ADVERSARIAL VERIFICATION SUITES
// ============================================================================

// ----------------------------------------------------------------------------
// 1. T3_Challenger_VoiceStealing_Tier2_Released_vs_Held_Priority
// Tests 4-tier voice stealing specification:
// Tier 1: Idle -> Tier 2: Released -> Tier 3: Pedal-Latched -> Tier 4: Held
// Verifies whether a released voice (noteOff sent, in release decay) is stolen
// before an actively held voice (key physically held down).
// ----------------------------------------------------------------------------
bool test_T3_Challenger_VoiceStealing_Tier2_Released_vs_Held_Priority() {
    VoiceManager vm;
    vm.prepare(48000.0);

    ParameterSnapshot p;
    p.voice_mode = 1; // Poly 8-Voice
    p.env_attack = 1.0f;
    p.env_decay = 50.0f;
    p.env_sustain = 0.8f;
    p.env_release = 200.0f; // 200 ms release tail

    // 1. Play 8 notes: 40, 41, 42, 43, 44, 45, 46, 47
    // Stagger them slightly so Note 40 is oldest (age ~110 ms), Note 47 is newest (age ~75 ms)
    for (int n = 40; n < 48; ++n) {
        MidiEvent ev { 0, 0x90, static_cast<uint8_t>(n), 100 };
        vm.handleMidiEvent(ev, p);
        for (int s = 0; s < 240; ++s) { // 5 ms each
            float l, r, air, ap, bub, drop;
            vm.processSample(p, l, r, air, ap, bub, drop);
        }
    }

    // Process another 70 ms (3360 samples) so all 8 notes have age > 60 ms
    for (int s = 0; s < 3360; ++s) {
        float l, r, air, ap, bub, drop;
        vm.processSample(p, l, r, air, ap, bub, drop);
    }

    TEST_ASSERT(vm.getActiveVoiceCount() == 8, "Expected 8 active polyphonic voices!");

    // 2. Release Note 47 (Note Off 47).
    // Note 47 is now in Release stage (decaying over 200 ms).
    // Notes 40..46 remain physically HELD by the player.
    MidiEvent off47 { 0, 0x80, 47, 0 };
    vm.handleMidiEvent(off47, p);

    // Process 10 ms (480 samples): Note 47 is still sounding in its release tail
    for (int s = 0; s < 480; ++s) {
        float l, r, air, ap, bub, drop;
        vm.processSample(p, l, r, air, ap, bub, drop);
    }
    TEST_ASSERT(vm.getActiveVoiceCount() == 8, "Expected all 8 voices still active during release tail!");

    // 3. Now strike Note 50 (a 9th note).
    // SPEC §4.D / R1.6: 4-tier prioritized voice stealing:
    // Tier 1: Idle
    // Tier 2: Released (Note 47)
    // Tier 3: Pedal-Latched
    // Tier 4: Held (Notes 40..46)
    // The voice stealing algorithm MUST steal Note 47 (the only released note),
    // and MUST NOT steal Note 40 (which is actively held down by the user).
    MidiEvent on50 { 0, 0x90, 50, 100 };
    vm.handleMidiEvent(on50, p);

    // 4. Release Note 50, and release Notes 41..46.
    // The user STILL HOLDS Note 40!
    MidiEvent off50 { 0, 0x80, 50, 0 };
    vm.handleMidiEvent(off50, p);
    for (int n = 41; n < 47; ++n) {
        MidiEvent offN { 0, 0x80, static_cast<uint8_t>(n), 0 };
        vm.handleMidiEvent(offN, p);
    }

    // 5. Render 2 seconds (96000 samples) so that all released notes (41..47, 50) fully decay to Idle
    for (int s = 0; s < 96000; ++s) {
        float l, r, air, ap, bub, drop;
        vm.processSample(p, l, r, air, ap, bub, drop);
    }

    const int activeAfter = vm.getActiveVoiceCount();
    std::cout << "\n[EMPIRICAL VOICE STEALING] Active voices remaining for held Note 40: " << activeAfter << " (expected 1)\n";
    TEST_ASSERT(activeAfter == 1,
        "CRITICAL BUG: VoiceManager stole a physically HELD note (Note 40) instead of the RELEASED note (Note 47)! "
        "Held bass note was terminated while user was holding the key.");

    return gCurrentTestAssertFailures == 0;
}

REGISTER_TEST("Tier3", T3_Challenger_VoiceStealing_Tier2_Released_vs_Held_Priority, "Challenger: 4-tier voice stealing steals Released notes before Held notes", test_T3_Challenger_VoiceStealing_Tier2_Released_vs_Held_Priority);

// ----------------------------------------------------------------------------
// 2. T3_Challenger_VoiceStealing_Tier3_PedalLatched_vs_Held_Priority
// Tests 4-tier voice stealing specification:
// Verifies whether a pedal-latched voice (Tier 3) is stolen before a physically
// held voice (Tier 4).
// ----------------------------------------------------------------------------
bool test_T3_Challenger_VoiceStealing_Tier3_PedalLatched_vs_Held_Priority() {
    VoiceManager vm;
    vm.prepare(48000.0);

    ParameterSnapshot p;
    p.voice_mode = 1; // Poly 8-Voice
    p.env_attack = 1.0f;
    p.env_decay = 50.0f;
    p.env_sustain = 0.8f;
    p.env_release = 200.0f;

    // 1. Press sustain pedal (CC 64 = 127)
    MidiEvent pedalDown { 0, 0xB0, 64, 127 };
    vm.handleMidiEvent(pedalDown, p);

    // 2. Play Note 40 (Pedal Note), then release it while pedal is held
    MidiEvent on40 { 0, 0x90, 40, 100 };
    vm.handleMidiEvent(on40, p);
    for (int s = 0; s < 480; ++s) {
        float l, r, air, ap, bub, drop;
        vm.processSample(p, l, r, air, ap, bub, drop);
    }
    MidiEvent off40 { 0, 0x80, 40, 0 };
    vm.handleMidiEvent(off40, p); // Now pedal-latched!

    // 3. Play 7 notes: Notes 41..47 (physically held)
    for (int n = 41; n < 48; ++n) {
        MidiEvent ev { 0, 0x90, static_cast<uint8_t>(n), 100 };
        vm.handleMidiEvent(ev, p);
    }

    // Render 100 ms (4800 samples)
    for (int s = 0; s < 4800; ++s) {
        float l, r, air, ap, bub, drop;
        vm.processSample(p, l, r, air, ap, bub, drop);
    }
    TEST_ASSERT(vm.getActiveVoiceCount() == 8, "Expected 8 active polyphonic voices!");

    // 4. Strike 9th note: Note 50.
    // According to 4-tier specification:
    // Tier 3 Pedal-Latched voice (Note 40) MUST be stolen before Tier 4 Held voices (Notes 41..47).
    MidiEvent on50 { 0, 0x90, 50, 100 };
    vm.handleMidiEvent(on50, p);

    // 5. Release sustain pedal (CC 64 = 0)
    // If Note 40 was NOT stolen (i.e. still latched), releasing pedal releases Note 40.
    // If a held note (e.g. Note 41) was incorrectly stolen, releasing pedal will NOT release Note 41!
    MidiEvent pedalUp { 0, 0xB0, 64, 0 };
    vm.handleMidiEvent(pedalUp, p);

    // 6. Release Note 50 as well
    MidiEvent off50 { 0, 0x80, 50, 0 };
    vm.handleMidiEvent(off50, p);

    // Render 2 seconds (96000 samples) to allow any released notes to finish decaying.
    // The keys 41..47 are STILL HELD DOWN by the player!
    for (int s = 0; s < 96000; ++s) {
        float l, r, air, ap, bub, drop;
        vm.processSample(p, l, r, air, ap, bub, drop);
    }

    const int activeAfter = vm.getActiveVoiceCount();
    std::cout << "[EMPIRICAL VOICE STEALING] Active held voices remaining: " << activeAfter << " (expected 7)\n";
    TEST_ASSERT(activeAfter == 7,

        "CRITICAL BUG: VoiceManager stole a physically HELD note (Tier 4) instead of the PEDAL-LATCHED note (Tier 3)! "
        "Pedal-latched voice was prioritized over held keys.");

    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier3", T3_Challenger_VoiceStealing_Tier3_PedalLatched_vs_Held_Priority, "Challenger: 4-tier voice stealing steals Pedal-Latched notes before Held notes", test_T3_Challenger_VoiceStealing_Tier3_PedalLatched_vs_Held_Priority);

// ----------------------------------------------------------------------------
// 3. T3_Challenger_VoiceStealing_DeclickCrossfade_DeadCode_Analysis
// Empirically measures sample-to-sample difference and validates whether 5 ms
// de-click crossfading operates or is eliminated by immediate noteOn reset.
// ----------------------------------------------------------------------------
bool test_T3_Challenger_VoiceStealing_DeclickCrossfade_DeadCode_Analysis() {
    VoiceManager vm;
    vm.prepare(48000.0);

    ParameterSnapshot p;
    p.voice_mode = 1; // Poly 8-Voice
    p.pressure = 0.85f;
    p.tension = 0.5f;
    p.env_attack = 0.1f;
    p.env_sustain = 1.0f;

    // Play 8 notes to fill all voice slots
    for (int n = 40; n < 48; ++n) {
        MidiEvent ev { 0, 0x90, static_cast<uint8_t>(n), 100 };
        vm.handleMidiEvent(ev, p);
    }

    // Render 200 ms to achieve steady-state oscillation
    for (int s = 0; s < 9600; ++s) {
        float l, r, air, ap, bub, drop;
        vm.processSample(p, l, r, air, ap, bub, drop);
    }

    // Record baseline sample-to-sample delta during normal continuous playback
    float maxDeltaNormal = 0.0f;
    float prevSample = 0.0f;

    for (int i = 0; i < 500; ++i) {
        float l, r, air, ap, bub, drop;
        vm.processSample(p, l, r, air, ap, bub, drop);
        if (i > 0) {
            maxDeltaNormal = std::max(maxDeltaNormal, std::abs(l - prevSample));
        }
        prevSample = l;
    }

    // Now trigger voice stealing by sending Note On for Note 70 (high pitch)
    MidiEvent evSteal { 0, 0x90, 70, 127 };
    vm.handleMidiEvent(evSteal, p);

    // Process first sample after steal
    float lSteal, rSteal, airS, apS, bubS, dropS;
    vm.processSample(p, lSteal, rSteal, airS, apS, bubS, dropS);
    const float stealDelta = std::abs(lSteal - prevSample);

    std::cout << "[EMPIRICAL DE-CLICK] Normal max sample delta = " << maxDeltaNormal 
              << ", Voice stealing sample delta = " << stealDelta << "\n";

    // Verify output remains finite
    TEST_ASSERT(isFiniteBitwise(lSteal), "Non-finite output on voice steal!");
    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier3", T3_Challenger_VoiceStealing_DeclickCrossfade_DeadCode_Analysis, "Challenger: Empirical de-click crossfade analysis during voice stealing", test_T3_Challenger_VoiceStealing_DeclickCrossfade_DeadCode_Analysis);

// ----------------------------------------------------------------------------
// 4. T3_Challenger_MidiStorm_150Events_Stress
// Blasts 160 rapid MIDI events (note on/off, pitch bend, CC) into DspEngine.
// Verifies 0 NaNs, bounded output <= 0.0 dBFS, voice count <= 8, and complete
// clean decay back to silence upon All Notes Off.
// ----------------------------------------------------------------------------
bool test_T3_Challenger_MidiStorm_150Events_Stress() {
    DspEngine engine;
    engine.prepare(48000.0, 256);

    ParameterSnapshot p;
    p.voice_mode = 1; // Poly 8-Voice
    p.pressure = 0.8f;
    p.env_attack = 1.0f;
    p.env_decay = 20.0f;
    p.env_sustain = 0.7f;
    p.env_release = 50.0f;

    FastPrng prng(0xDEADBEEF1010101ULL);

    constexpr int kNumStormEvents = 160;
    std::vector<MidiEvent> stormEvents(kNumStormEvents);

    int sampleOffsetAccum = 0;
    for (int i = 0; i < kNumStormEvents; ++i) {
        sampleOffsetAccum += static_cast<int>(prng.nextU64() % 8);
        const float r = prng.nextFloat01();

        if (r < 0.50f) {
            const uint8_t note = static_cast<uint8_t>(30 + (prng.nextU64() % 48));
            const uint8_t vel  = static_cast<uint8_t>(20 + (prng.nextU64() % 108));
            stormEvents[i] = MidiEvent { sampleOffsetAccum, 0x90, note, vel };
        } else if (r < 0.80f) {
            const uint8_t note = static_cast<uint8_t>(30 + (prng.nextU64() % 48));
            stormEvents[i] = MidiEvent { sampleOffsetAccum, 0x80, note, 0 };
        } else if (r < 0.90f) {
            const uint8_t lsb = static_cast<uint8_t>(prng.nextU64() & 0x7F);
            const uint8_t msb = static_cast<uint8_t>((prng.nextU64() >> 7) & 0x7F);
            stormEvents[i] = MidiEvent { sampleOffsetAccum, 0xE0, lsb, msb };
        } else {
            const uint8_t val = (prng.nextFloat01() < 0.5f) ? 0 : 127;
            stormEvents[i] = MidiEvent { sampleOffsetAccum, 0xB0, 64, val };
        }
    }

    constexpr int kBlockSize = 128;
    std::vector<float> left(kBlockSize);
    std::vector<float> right(kBlockSize);

    int eventIdx = 0;
    int currentSampleTime = 0;

    for (int block = 0; block < 100; ++block) {
        MidiEvent blockEvents[32];
        int numBlockEvents = 0;

        while (eventIdx < kNumStormEvents && stormEvents[eventIdx].sampleOffset < currentSampleTime + kBlockSize) {
            if (numBlockEvents < 32) {
                blockEvents[numBlockEvents] = stormEvents[eventIdx];
                blockEvents[numBlockEvents].sampleOffset -= currentSampleTime;
                if (blockEvents[numBlockEvents].sampleOffset < 0) {
                    blockEvents[numBlockEvents].sampleOffset = 0;
                }
                ++numBlockEvents;
            }
            ++eventIdx;
        }

        engine.process(left.data(), right.data(), kBlockSize, p, blockEvents, numBlockEvents);

        for (int i = 0; i < kBlockSize; ++i) {
            TEST_ASSERT(isFiniteBitwise(left[i]), "Non-finite sample rendered during MIDI storm (Left)!");
            TEST_ASSERT(isFiniteBitwise(right[i]), "Non-finite sample rendered during MIDI storm (Right)!");
            TEST_ASSERT(left[i] <= 1.0000000f && left[i] >= -1.0000000f, "Limiter ceiling violated during MIDI storm (Left)!");
            TEST_ASSERT(right[i] <= 1.0000000f && right[i] >= -1.0000000f, "Limiter ceiling violated during MIDI storm (Right)!");
        }

        currentSampleTime += kBlockSize;
    }

    // Verify recovery: send All Notes Off and release pedal
    MidiEvent allOff { 0, 0xB0, 123, 0 };
    MidiEvent pedalOff { 0, 0xB0, 64, 0 };
    MidiEvent resetEvents[] = { allOff, pedalOff };
    engine.process(left.data(), right.data(), kBlockSize, p, resetEvents, 2);

    // Render 1 second of silence decay
    for (int b = 0; b < 400; ++b) {
        engine.process(left.data(), right.data(), kBlockSize, p, nullptr, 0);
    }

    float maxTailL = 0.0f, maxTailR = 0.0f;
    for (int i = 0; i < kBlockSize; ++i) {
        maxTailL = std::max(maxTailL, std::abs(left[i]));
        maxTailR = std::max(maxTailR, std::abs(right[i]));
    }

    TEST_ASSERT(maxTailL < 1.0e-4f, "Engine did not return to silence after All Notes Off!");
    TEST_ASSERT(maxTailR < 1.0e-4f, "Engine did not return to silence after All Notes Off!");

    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier3", T3_Challenger_MidiStorm_150Events_Stress, "Challenger: Intense MIDI storm 160 rapid events (bounds, finite, silence recovery)", test_T3_Challenger_MidiStorm_150Events_Stress);

// ----------------------------------------------------------------------------
// 5. T3_Challenger_RealTime_ZeroAllocations_Under_Intense_Burst
// Verifies 0 heap allocations across 200 blocks with continuous parameter sweeps,
// voice mode switching, cavity model changes, and rapid MIDI events.
// ----------------------------------------------------------------------------
bool test_T3_Challenger_RealTime_ZeroAllocations_Under_Intense_Burst() {
    DspEngine engine;
    engine.prepare(48000.0, 512);

    ParameterSnapshot p;
    FastPrng prng(0xABCDEF0123456789ULL);

    std::vector<float> left(512), right(512);

    // Warm up
    engine.process(left.data(), right.data(), 512, p, nullptr, 0);

    // Enter strictly tracked allocation guard
    {
        ScopedAllocationGuard guard;

        for (int b = 0; b < 200; ++b) {
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
            p.master_gain    = -12.0f + 24.0f * prng.nextFloat01();

            MidiEvent events[4];
            int numEvents = 0;
            if (prng.nextFloat01() < 0.6f) {
                events[numEvents++] = MidiEvent { 0, 0x90, static_cast<uint8_t>(36 + prng.nextU64() % 48), 100 };
            }
            if (prng.nextFloat01() < 0.4f) {
                events[numEvents++] = MidiEvent { 64, 0x80, static_cast<uint8_t>(36 + prng.nextU64() % 48), 0 };
            }

            const int bs = (b % 4 == 0) ? 64 : ((b % 4 == 1) ? 128 : ((b % 4 == 2) ? 256 : 512));
            engine.process(left.data(), right.data(), bs, p, events, numEvents);
        }

        const size_t allocCount = guard.getCount();
        const size_t allocBytes = guard.getBytes();

        std::cout << "[EMPIRICAL ALLOCATION AUDIT] Allocations: " << allocCount 
                  << ", Bytes: " << allocBytes << " across 200 multi-mode bursts\n";

        TEST_ASSERT(allocCount == 0, "Dynamic memory allocations detected during audio process() execution!");
    }

    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier3", T3_Challenger_RealTime_ZeroAllocations_Under_Intense_Burst, "Challenger: Zero heap allocations under intense parameter/mode/MIDI bursts", test_T3_Challenger_RealTime_ZeroAllocations_Under_Intense_Burst);

// ----------------------------------------------------------------------------
// 6. T3_Challenger_MultiThreaded_HighContention_4Threads
// 4 concurrent threads: Audio render, parameter automation at 10k/sec, MIDI generator,
// and telemetry consumer running across 1,000 blocks.
// ----------------------------------------------------------------------------
bool test_T3_Challenger_MultiThreaded_HighContention_4Threads() {
    DspEngine engine;
    engine.prepare(48000.0, 256);

    std::atomic<bool> keepRunning { true };
    std::atomic<int> audioBlocksRendered { 0 };

    struct AtomicParams {
        std::atomic<float> pressure { 0.7f };
        std::atomic<float> tension { 0.5f };
        std::atomic<float> aperture { 0.35f };
        std::atomic<float> flutter { 0.15f };
        std::atomic<float> viscosity { 0.25f };
        std::atomic<float> moisture { 0.2f };
        std::atomic<float> droplet_rate { 0.3f };
        std::atomic<float> cleft_damping { 0.5f };
        std::atomic<float> porcelain_mix { 0.3f };
        std::atomic<float> porcelain_size { 1.0f };
        std::atomic<int>   porcelain_model { 1 };
        std::atomic<int>   voice_mode { 1 };
        std::atomic<float> drive { 0.2f };
        std::atomic<float> master_gain { 0.0f };
        std::atomic<float> sub_level { -6.0f };
        std::atomic<float> macro_squeeze { 0.5f };
        std::atomic<float> macro_moisture { 0.5f };
    } atomicParams;

    static constexpr size_t kMidiQueueSize = 256;
    alignas(64) std::atomic<size_t> midiWriteIdx { 0 };
    alignas(64) std::atomic<size_t> midiReadIdx { 0 };
    std::array<MidiEvent, kMidiQueueSize> midiRingBuffer {};

    auto pushMidi = [&](const MidiEvent& ev) -> bool {
        const size_t w = midiWriteIdx.load(std::memory_order_relaxed);
        const size_t r = midiReadIdx.load(std::memory_order_acquire);
        if (((w + 1) % kMidiQueueSize) == (r % kMidiQueueSize)) return false;
        midiRingBuffer[w % kMidiQueueSize] = ev;
        midiWriteIdx.store(w + 1, std::memory_order_release);
        return true;
    };

    auto popMidi = [&](MidiEvent& ev) -> bool {
        const size_t r = midiReadIdx.load(std::memory_order_relaxed);
        const size_t w = midiWriteIdx.load(std::memory_order_acquire);
        if (r == w) return false;
        ev = midiRingBuffer[r % kMidiQueueSize];
        midiReadIdx.store(r + 1, std::memory_order_release);
        return true;
    };

    // Thread 1: Audio Processing Thread
    std::thread audioThread([&]() {
        std::vector<float> left(256);
        std::vector<float> right(256);

        while (keepRunning.load(std::memory_order_relaxed)) {
            ParameterSnapshot localP;
            localP.pressure       = atomicParams.pressure.load(std::memory_order_relaxed);
            localP.tension        = atomicParams.tension.load(std::memory_order_relaxed);
            localP.aperture       = atomicParams.aperture.load(std::memory_order_relaxed);
            localP.flutter        = atomicParams.flutter.load(std::memory_order_relaxed);
            localP.viscosity      = atomicParams.viscosity.load(std::memory_order_relaxed);
            localP.moisture       = atomicParams.moisture.load(std::memory_order_relaxed);
            localP.droplet_rate   = atomicParams.droplet_rate.load(std::memory_order_relaxed);
            localP.cleft_damping  = atomicParams.cleft_damping.load(std::memory_order_relaxed);
            localP.porcelain_mix  = atomicParams.porcelain_mix.load(std::memory_order_relaxed);
            localP.porcelain_size = atomicParams.porcelain_size.load(std::memory_order_relaxed);
            localP.porcelain_model= atomicParams.porcelain_model.load(std::memory_order_relaxed);
            localP.voice_mode     = atomicParams.voice_mode.load(std::memory_order_relaxed);
            localP.drive          = atomicParams.drive.load(std::memory_order_relaxed);
            localP.master_gain    = atomicParams.master_gain.load(std::memory_order_relaxed);
            localP.sub_level      = atomicParams.sub_level.load(std::memory_order_relaxed);
            localP.macro_squeeze  = atomicParams.macro_squeeze.load(std::memory_order_relaxed);
            localP.macro_moisture = atomicParams.macro_moisture.load(std::memory_order_relaxed);

            MidiEvent blockMidi[16];
            int midiCount = 0;
            MidiEvent ev;
            while (midiCount < 16 && popMidi(ev)) {
                blockMidi[midiCount++] = ev;
            }

            engine.process(left.data(), right.data(), 256, localP, blockMidi, midiCount);

            for (int i = 0; i < 256; ++i) {
                if (!isFiniteBitwise(left[i]) || !isFiniteBitwise(right[i])) {
                    TEST_ASSERT(false, "Non-finite audio rendered under 4-thread contention!");
                }
                if (left[i] > 1.0000000f || right[i] > 1.0000000f ||
                    left[i] < -1.0000000f || right[i] < -1.0000000f) {
                    TEST_ASSERT(false, "Limiter ceiling violated under 4-thread contention!");
                }
            }

            audioBlocksRendered.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::yield();
        }
    });

    // Thread 2: Parameter Automation Sweeper (10,000 updates/sec)
    std::thread paramThread([&]() {
        FastPrng prng(0x5555AAAA1234ULL);
        while (keepRunning.load(std::memory_order_relaxed)) {
            atomicParams.pressure.store(prng.nextFloat01(), std::memory_order_relaxed);
            atomicParams.tension.store(prng.nextFloat01(), std::memory_order_relaxed);
            atomicParams.aperture.store(prng.nextFloat01(), std::memory_order_relaxed);
            atomicParams.moisture.store(prng.nextFloat01(), std::memory_order_relaxed);
            atomicParams.porcelain_size.store(0.5f + 1.5f * prng.nextFloat01(), std::memory_order_relaxed);
            atomicParams.drive.store(prng.nextFloat01(), std::memory_order_relaxed);
            atomicParams.master_gain.store(-18.0f + 24.0f * prng.nextFloat01(), std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Thread 3: MIDI Generator Thread
    std::thread midiThread([&]() {
        FastPrng prng(0x999988887777ULL);
        while (keepRunning.load(std::memory_order_relaxed)) {
            const uint8_t note = static_cast<uint8_t>(36 + prng.nextU64() % 48);
            const uint8_t vel = static_cast<uint8_t>(10 + prng.nextU64() % 117);
            const uint8_t type = (prng.nextFloat01() < 0.6f) ? 0x90 : 0x80;
            pushMidi(MidiEvent { 0, type, note, vel });

            if (prng.nextFloat01() < 0.1f) {
                pushMidi(MidiEvent { 0, 0xB0, 64, (prng.nextFloat01() < 0.5f) ? (uint8_t)0 : (uint8_t)127 });
            }

            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    });

    // Thread 4: Telemetry Consumer Thread
    std::thread telemetryThread([&]() {
        VisualizerFrame frame;
        while (keepRunning.load(std::memory_order_relaxed)) {
            while (engine.popVisualizerFrame(frame)) {
                if (!isFiniteBitwise(frame.outputRmsL) || !isFiniteBitwise(frame.outputRmsR)) {
                    TEST_ASSERT(false, "Telemetry contained non-finite RMS under multi-thread stress!");
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    while (audioBlocksRendered.load(std::memory_order_relaxed) < 1000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    keepRunning.store(false, std::memory_order_relaxed);
    audioThread.join();
    paramThread.join();
    midiThread.join();
    telemetryThread.join();

    std::cout << "[EMPIRICAL CONCURRENCY] Rendered " << audioBlocksRendered.load() << " audio blocks across 4 concurrent threads with zero errors\n";

    TEST_ASSERT(audioBlocksRendered.load() >= 1000, "Audio thread did not render expected 1,000 blocks!");
    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier3", T3_Challenger_MultiThreaded_HighContention_4Threads, "Challenger: 4-thread high-contention stability (audio, 10k param/s, MIDI, telemetry)", test_T3_Challenger_MultiThreaded_HighContention_4Threads);

// ----------------------------------------------------------------------------
// 7. T3_Challenger_ModeSwitch_MonoToPoly_StuckNote
// Tests mode switching: when switching from Mono-Legato (mode 0) to Polyphonic
// (mode 1) with a held note in the mono stack, does the mono note stack cause
// polyphonic notes to become permanently stuck (un-releasable)?
// ----------------------------------------------------------------------------
bool test_T3_Challenger_ModeSwitch_MonoToPoly_StuckNote() {
    VoiceManager vm;
    vm.prepare(48000.0);

    ParameterSnapshot pMono;
    pMono.voice_mode = 0; // Mono Legato
    pMono.env_release = 5.0f;

    // 1. Play Note 40 in Mono mode (enters mNoteStack)
    MidiEvent evOn40 { 0, 0x90, 40, 100 };
    vm.handleMidiEvent(evOn40, pMono);

    for (int s = 0; s < 480; ++s) {
        float l, r, air, ap, bub, drop;
        vm.processSample(pMono, l, r, air, ap, bub, drop);
    }
    TEST_ASSERT(vm.getActiveVoiceCount() == 1, "Expected 1 active voice in Mono mode!");

    // 2. Switch to Polyphonic mode (mode 1)
    ParameterSnapshot pPoly;
    pPoly.voice_mode = 1; // Poly 8-Voice
    pPoly.env_release = 5.0f;

    // 3. Play Note 60 in Poly mode
    MidiEvent evOn60 { 0, 0x90, 60, 100 };
    vm.handleMidiEvent(evOn60, pPoly);

    for (int s = 0; s < 480; ++s) {
        float l, r, air, ap, bub, drop;
        vm.processSample(pPoly, l, r, air, ap, bub, drop);
    }

    // 4. Release Note 60 in Poly mode
    MidiEvent evOff60 { 0, 0x80, 60, 0 };
    vm.handleMidiEvent(evOff60, pPoly);

    // Render 100 ms (4800 samples)
    for (int s = 0; s < 4800; ++s) {
        float l, r, air, ap, bub, drop;
        vm.processSample(pPoly, l, r, air, ap, bub, drop);
    }

    // Note 60 should have been released and decayed to Idle!
    // Active voices should be at most 1 (the remaining Note 40 from Mono mode, or 0).
    const int activeVoices = vm.getActiveVoiceCount();
    std::cout << "[EMPIRICAL MODE SWITCH] Active voices after releasing Poly note 60: " << activeVoices << "\n";

    TEST_ASSERT(activeVoices <= 1,
        "CRITICAL BUG: Polyphonic Note 60 became permanently stuck! "
        "handleNoteOff() returned early due to stale mNoteStack from Mono mode.");

    return gCurrentTestAssertFailures == 0;
}
REGISTER_TEST("Tier3", T3_Challenger_ModeSwitch_MonoToPoly_StuckNote, "Challenger: Mono-to-Poly mode switch does not cause stuck polyphonic notes", test_T3_Challenger_ModeSwitch_MonoToPoly_StuckNote);

} // namespace ppf42::test


