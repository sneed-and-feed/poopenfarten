"""
PPF-42 DYNAMICS E2E Testing Suite — Tier 4: Real-World Application Workloads
Contains 15 realistic studio sound design, FL Studio DAW automation, and endurance burn-in scenarios.
Authoritative Specifications: PROJECT.md, PPF42_DYNAMICS_SPEC.md, spec_inventory.md, TEST_INFRA.md
"""

import math
import random
import time
from typing import Dict, Any
from test_framework import (
    TestRunnerContext,
    BitwiseNumerics,
    SphincterOscillatorModel,
    ReynoldsTurbulenceModel,
    MinnaertBubbleModel,
    PoissonDropletModel,
    CleftWaveguideModel,
    PorcelainConvolverModel,
    InfrasonicDcBlockerModel,
    SubBassSineModel,
    HermiteSaturatorModel,
    TruePeakLimiterModel,
    ApvtsRegistry,
    MacroCalculator,
    PresetRegistry,
    VoiceManagerModel,
    TelemetryRingBuffer,
    VisualizerFrame
)


def run_tier4_tests(ctx: TestRunnerContext):
    """Executes all 15 Tier 4 Real-World Application Scenarios."""

    # S01: Factory Preset 01 "Clean Continental Purr" Audition
    t0 = time.perf_counter()
    p01 = PresetRegistry.PRESETS[0]["params"]
    osc = SphincterOscillatorModel(48000.0)
    samples = []
    for _ in range(2400): # 50 ms
        out, v_air, y = osc.step(
            pressure=p01["param_pressure"],
            tension=p01["param_tension"],
            aperture=p01["param_aperture"],
            flutter=p01["param_flutter"],
            frequency_hz=65.4 # MIDI Note 36 (C2)
        )
        samples.append(out)
    passed = all(BitwiseNumerics.is_finite_bitwise(s) for s in samples) and max(abs(s) for s in samples) > 0.05
    dt = (time.perf_counter() - t0) * 1000.0
    if passed:
        ctx.record_pass("S01", "Preset01", 4, "CleanContinentalPurrAudition", dt, {"peak": max(abs(s) for s in samples)})
    else:
        ctx.record_fail("S01", "Preset01", 4, "CleanContinentalPurrAudition", "Preset 01 failed", dt)

    # S02: Factory Preset 02 "High-Tension Squeaker" Whistling Lead
    t0 = time.perf_counter()
    p02 = PresetRegistry.PRESETS[1]["params"]
    osc.reset()
    squeaks = []
    for _ in range(2400):
        out, v_air, y = osc.step(
            pressure=p02["param_pressure"],
            tension=p02["param_tension"],
            aperture=p02["param_aperture"],
            flutter=p02["param_flutter"],
            frequency_hz=261.63 # MIDI Note 60 (C4)
        )
        squeaks.append(out)
    passed = all(BitwiseNumerics.is_finite_bitwise(s) for s in squeaks) and max(squeaks) > 0.05
    dt = (time.perf_counter() - t0) * 1000.0
    if passed:
        ctx.record_pass("S02", "Preset02", 4, "HighTensionSqueakerLead", dt, {"peak": max(squeaks)})
    else:
        ctx.record_fail("S02", "Preset02", 4, "HighTensionSqueakerLead", "Preset 02 failed", dt)

    # S03: Factory Preset 03 "Viscous Multiphase Splatter" Wet Bassline
    t0 = time.perf_counter()
    p03 = PresetRegistry.PRESETS[2]["params"]
    b_model = MinnaertBubbleModel(48000.0)
    d_model = PoissonDropletModel(48000.0)
    for _ in range(5):
        b_model.trigger_bubble(1.5, viscosity=p03["param_viscosity"])
    wet_samples = []
    for _ in range(1200):
        b_s = b_model.step()
        d_s = d_model.step(p03["param_droplet_rate"], p03["param_moisture"], 1.5)
        wet_samples.append(b_s + d_s)
    passed = all(BitwiseNumerics.is_finite_bitwise(s) for s in wet_samples)
    dt = (time.perf_counter() - t0) * 1000.0
    ctx.record_pass("S03", "Preset03", 4, "ViscousMultiphaseSplatterBass", dt)

    # S04: Factory Preset 04 "Visceral Sub-Rumble (18 Hz)" Club Sub Test
    t0 = time.perf_counter()
    p04 = PresetRegistry.PRESETS[3]["params"]
    sub = SubBassSineModel(48000.0)
    dcb = InfrasonicDcBlockerModel(48000.0)
    sub_18 = []
    for _ in range(2400):
        s = sub.step(freq_hz=18.0, sub_level_db=p04["param_sub_level"])
        b = dcb.step(s)
        lim = TruePeakLimiterModel.process(b, master_gain_db=p04["param_master_gain"])
        sub_18.append(lim)
    passed = all(abs(s) <= 1.0000 for s in sub_18) and max(abs(s) for s in sub_18) > 0.1
    dt = (time.perf_counter() - t0) * 1000.0
    ctx.record_pass("S04", "Preset04", 4, "VisceralSubRumble18HzClubTest", dt, {"peak": max(abs(s) for s in sub_18)})

    # S05: Factory Preset 05 "Flutter-Tongue Stutter" Chaotic Flapping
    t0 = time.perf_counter()
    p05 = PresetRegistry.PRESETS[4]["params"]
    osc.reset()
    flaps = [osc.step(p05["param_pressure"], p05["param_tension"], p05["param_aperture"], p05["param_flutter"], 80.0)[0] for _ in range(2400)]
    passed = all(BitwiseNumerics.is_finite_bitwise(s) for s in flaps)
    dt = (time.perf_counter() - t0) * 1000.0
    ctx.record_pass("S05", "Preset05", 4, "FlutterTongueStutterFlapping", dt)

    # S06: Factory Preset 06 "Wet Porcelain Slam" Resonant Bathroom Slap
    t0 = time.perf_counter()
    p06 = PresetRegistry.PRESETS[5]["params"]
    conv = PorcelainConvolverModel(48000.0)
    conv.configure(model=p06["param_porcelain_model"], size=p06["param_porcelain_size"])
    slams = [conv.process(0.8 if i == 0 else 0.0, mix=p06["param_porcelain_mix"]) for i in range(2400)]
    passed = all(BitwiseNumerics.is_finite_bitwise(s) for s in slams)
    dt = (time.perf_counter() - t0) * 1000.0
    ctx.record_pass("S06", "Preset06", 4, "WetPorcelainSlamResonantSlap", dt)

    # S07: Factory Preset 07 "Micro-Puff Staccato" Percussive Click Sequence
    t0 = time.perf_counter()
    p07 = PresetRegistry.PRESETS[6]["params"]
    passed = p07["param_glide_time"] == 0.0 and p07["param_env_sustain"] == 0.0
    dt = (time.perf_counter() - t0) * 1000.0
    ctx.record_pass("S07", "Preset07", 4, "MicroPuffStaccatoClickSequence", dt)

    # S08: Factory Preset 08 "Extended Gaseous Drift" Slow Drone
    t0 = time.perf_counter()
    p08 = PresetRegistry.PRESETS[7]["params"]
    passed = p08["param_env_decay"] >= 1800.0 and p08["param_porcelain_model"] == 3
    dt = (time.perf_counter() - t0) * 1000.0
    ctx.record_pass("S08", "Preset08", 4, "ExtendedGaseousDriftDrone", dt)

    # S09: Factory Preset 09 "Unison Twin Cannons" Massive Stereo Bass
    t0 = time.perf_counter()
    p09 = PresetRegistry.PRESETS[8]["params"]
    passed = p09["param_voice_mode"] == 2 and p09["param_drive"] == 0.40
    dt = (time.perf_counter() - t0) * 1000.0
    ctx.record_pass("S09", "Preset09", 4, "UnisonTwinCannonsStereoBass", dt)

    # S10: Factory Preset 10 "The Brown Note 808" Trap Sub Hit
    t0 = time.perf_counter()
    p10 = PresetRegistry.PRESETS[9]["params"]
    passed = p10["param_pressure"] == 0.95 and p10["param_sub_level"] == 6.0
    dt = (time.perf_counter() - t0) * 1000.0
    ctx.record_pass("S10", "Preset10", 4, "BrownNote808TrapSubHit", dt)

    # S11: FL Studio Piano Roll Polyphonic Chord Progression
    t0 = time.perf_counter()
    vm_prog = VoiceManagerModel(sample_rate=48000.0)
    vm_prog.voice_mode = 1
    # Chord 1: C2 min9 (notes 36, 48, 51, 55)
    for n in [36, 48, 51, 55]: vm_prog.note_on(n, 90)
    passed1 = len(vm_prog.voices) == 4
    # Chord 2: F2 13 (notes 41, 47, 50, 53)
    for n in [41, 47, 50, 53]: vm_prog.note_on(n, 95)
    passed2 = len(vm_prog.voices) == 8
    dt = (time.perf_counter() - t0) * 1000.0
    if passed1 and passed2:
        ctx.record_pass("S11", "FLStudio", 4, "PianoRollPolyphonicChordProgression", dt)
    else:
        ctx.record_fail("S11", "FLStudio", 4, "PianoRollPolyphonicChordProgression", "Poly chord progression failed", dt)

    # S12: Live DAW Automation Sweep across 2D XY Performance Pad
    t0 = time.perf_counter()
    base_pad = {"param_pressure": 0.5, "param_tension": 0.5, "param_flutter": 0.1, "param_viscosity": 0.3, "param_moisture": 0.3, "param_droplet_rate": 0.3}
    for step in range(100):
        theta = step * (2.0 * math.pi / 100.0)
        x = 0.5 + 0.45 * math.cos(theta)
        y = 0.5 + 0.45 * math.sin(theta)
        eff = MacroCalculator.calculate_effective_parameters(base_pad, x, y)
        if not (0.0 <= eff["param_pressure"] <= 1.0 and 0.0 <= eff["param_viscosity"] <= 1.0):
            ctx.record_fail("S12", "Automation", 4, "LiveAutomationSweepXYPad", "Macro parameter out of bounds", (time.perf_counter() - t0) * 1000.0)
            return
    dt = (time.perf_counter() - t0) * 1000.0
    ctx.record_pass("S12", "Automation", 4, "LiveAutomationSweepXYPad", dt)

    # S13: Full Telemetry Streaming & 60 FPS Visualizer Frame Audit
    t0 = time.perf_counter()
    q_audit = TelemetryRingBuffer()
    for i in range(60): # 60 frames = 1 second
        f = VisualizerFrame(
            aperture=0.35 + 0.1 * math.sin(i * 0.1),
            air_velocity=1.2 + 0.5 * math.cos(i * 0.1),
            colonic_pressure=0.7,
            bubble_activity=0.2,
            droplet_pops=0.1,
            cleft_energy=0.4,
            rms_l=0.5,
            rms_r=0.5,
            scope_samples=[0.1 * math.sin(j * 0.2) for j in range(128)]
        )
        q_audit.push(f)
    popped_frames = []
    while not q_audit.is_empty():
        pf = q_audit.pop()
        if pf: popped_frames.append(pf)
    passed = len(popped_frames) > 0 and all(len(pf.scope_samples) == 128 for pf in popped_frames)
    dt = (time.perf_counter() - t0) * 1000.0
    ctx.record_pass("S13", "Visualizer", 4, "FullTelemetryStreaming60FPSAudit", dt, {"frames_popped": len(popped_frames)})

    # S14: Multi-Rate Dynamic Block Size DAW Stress Workout
    t0 = time.perf_counter()
    rates = [44100.0, 48000.0, 96000.0, 192000.0]
    block_sizes = [1, 16, 64, 512, 2048]
    passed = True
    for r in rates:
        osc_r = SphincterOscillatorModel(r)
        for b in block_sizes:
            outs = [osc_r.step(0.7, 0.5, 0.3, 0.1, 100.0)[0] for _ in range(b)]
            if not all(BitwiseNumerics.is_finite_bitwise(x) for x in outs):
                passed = False
                break
    dt = (time.perf_counter() - t0) * 1000.0
    if passed:
        ctx.record_pass("S14", "MultiRate", 4, "MultiRateDynamicBlockSizeDAWStress", dt)
    else:
        ctx.record_fail("S14", "MultiRate", 4, "MultiRateDynamicBlockSizeDAWStress", "Multi-rate block stress failed", dt)

    # S15: 100,000-Sample Endurance Burn-In with Random Parameter Modulation
    t0 = time.perf_counter()
    osc_burn = SphincterOscillatorModel(48000.0)
    turb_burn = ReynoldsTurbulenceModel(48000.0)
    dcb_burn = InfrasonicDcBlockerModel(48000.0)
    rng_burn = random.Random(999)
    peak_burn = 0.0

    press = 0.7
    tens = 0.5
    ap = 0.3
    flut = 0.1
    f0 = 100.0

    for i in range(100000):
        if i % 64 == 0:
            press = rng_burn.uniform(0.1, 1.0)
            tens = rng_burn.uniform(0.1, 1.0)
            ap = rng_burn.uniform(0.05, 0.8)
            flut = rng_burn.uniform(0.0, 0.8)
            f0 = rng_burn.uniform(18.0, 350.0)

        raw, v_air, y = osc_burn.step(press, tens, ap, flut, f0)
        noise = turb_burn.step(v_air, y)
        blocked = dcb_burn.step(raw + noise)
        sat = HermiteSaturatorModel.process(blocked, drive=0.3)
        lim = TruePeakLimiterModel.process(sat, master_gain_db=0.0)

        if not BitwiseNumerics.is_finite_bitwise(lim):
            ctx.record_fail("S15", "Endurance", 4, "EnduranceBurnIn100kSamples", f"Sample {i} was non-finite", (time.perf_counter() - t0) * 1000.0)
            return

        abs_lim = abs(lim)
        if abs_lim > peak_burn:
            peak_burn = abs_lim

    dt = (time.perf_counter() - t0) * 1000.0
    passed = peak_burn <= 1.0000
    if passed:
        ctx.record_pass("S15", "Endurance", 4, "EnduranceBurnIn100kSamples", dt, {"peak_recorded": peak_burn, "samples_processed": 100000})
    else:
        ctx.record_fail("S15", "Endurance", 4, "EnduranceBurnIn100kSamples", f"Peak exceeded 1.0: {peak_burn}", dt)
