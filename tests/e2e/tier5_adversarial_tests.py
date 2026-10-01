"""
PPF-42 DYNAMICS E2E Testing Suite — Tier 5: Adversarial Coverage Hardening
Implements white-box stress testing, extreme parameter overloads, rapid preset switching,
peak limiter verification, zero-denormal auditing, and standalone binary smoke tests.
Authoritative Specifications: PROJECT.md, PPF42_DYNAMICS_SPEC.md, DISPATCH.md
"""

import math
import random
import time
import subprocess
import os
from typing import Dict, Any, List
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


def run_tier5_tests(ctx: TestRunnerContext):
    """Executes all Tier 5 Adversarial Coverage Hardening Tests."""

    # --------------------------------------------------------------------------
    # ADV-01: Rapid Preset Switching During Active Note Rendering
    # --------------------------------------------------------------------------
    t0 = time.perf_counter()
    sample_rate = 48000.0
    vm = VoiceManagerModel(max_voices=8, sample_rate=sample_rate)
    vm.voice_mode = 1 # Polyphonic
    vm.note_on(36, 110)
    vm.note_on(48, 100)
    vm.note_on(55, 95)
    vm.note_on(60, 90)

    wg = CleftWaveguideModel(sample_rate)
    conv = PorcelainConvolverModel(sample_rate)
    dc = InfrasonicDcBlockerModel(sample_rate)
    limiter = TruePeakLimiterModel()

    all_finite = True
    no_denormals = True
    all_bounded = True
    max_peak = 0.0

    # Rapidly cycle through all 10 presets across 100 blocks (12,800 samples)
    for block in range(100):
        preset_idx = block % 10
        p = PresetRegistry.PRESETS[preset_idx]["params"]
        eff_p = MacroCalculator.calculate_effective_parameters(p, p.get("macro_squeeze", 0.5), p.get("macro_moisture", 0.2))

        conv.configure(int(eff_p.get('param_porcelain_model', 1)), float(eff_p.get('param_porcelain_size', 1.0)))

        for _ in range(128):
            v_air = 0.5
            ap = 0.3
            # Simulate voice rendering
            v_out = 0.0
            for v in vm.voices:
                out_s, va, a = v['osc'].step(
                    pressure=eff_p['param_pressure'],
                    tension=eff_p['param_tension'],
                    aperture=eff_p['param_aperture'],
                    flutter=eff_p['param_flutter'],
                    frequency_hz=v['freq']
                )
                v_out += out_s

            v_out *= 0.35
            w_out = wg.step(v_out, eff_p['param_cleft_damping'])
            c_out = conv.process(w_out, eff_p['param_porcelain_mix'])
            dc_out = dc.step(c_out)
            sat_out = HermiteSaturatorModel.process(dc_out, eff_p['param_drive'])
            master_linear = math.pow(10.0, eff_p['param_master_gain'] / 20.0)
            final_sample = limiter.process(sat_out * master_linear)

            if not BitwiseNumerics.is_finite_bitwise(final_sample):
                all_finite = False
            if BitwiseNumerics.is_denormal(final_sample):
                no_denormals = False
            if abs(final_sample) > 1.000001:
                all_bounded = False
            if abs(final_sample) > max_peak:
                max_peak = abs(final_sample)

    passed_01 = all_finite and no_denormals and all_bounded and max_peak > 0.01
    dt_01 = (time.perf_counter() - t0) * 1000.0
    if passed_01:
        ctx.record_pass("ADV01", "F25+F15", 5, "RapidPresetSwitchingActiveNotes", dt_01, {"max_peak": max_peak})
    else:
        ctx.record_fail("ADV01", "F25+F15", 5, "RapidPresetSwitchingActiveNotes",
                        f"Rapid preset switching failed: finite={all_finite}, no_denorm={no_denormals}, bounded={all_bounded}", dt_01)

    # --------------------------------------------------------------------------
    # ADV-02: Extreme Parameter Combinations (Squeeze 1.0 + Moisture 1.0 + Drive 1.0)
    # --------------------------------------------------------------------------
    t0 = time.perf_counter()
    extreme_p = {
        "param_pressure": 1.0, "param_tension": 1.0, "param_aperture": 1.0, "param_flutter": 1.0,
        "param_viscosity": 1.0, "param_moisture": 1.0, "param_droplet_rate": 1.0, "param_cleft_damping": 0.0,
        "param_porcelain_mix": 1.0, "param_porcelain_size": 2.0, "param_porcelain_model": 1, "param_voice_mode": 1,
        "param_glide_time": 0.0, "param_sub_level": 6.0, "param_drive": 1.0, "param_master_gain": 12.0,
        "macro_squeeze": 1.0, "macro_moisture": 1.0,
        "param_env_attack": 0.1, "param_env_decay": 2000.0, "param_env_sustain": 1.0, "param_env_release": 2000.0
    }
    eff_extreme = MacroCalculator.calculate_effective_parameters(extreme_p, 1.0, 1.0)

    osc = SphincterOscillatorModel(sample_rate)
    turb = ReynoldsTurbulenceModel(sample_rate)
    bubble = MinnaertBubbleModel(sample_rate)
    droplet = PoissonDropletModel(sample_rate)
    wg_ext = CleftWaveguideModel(sample_rate)
    conv_ext = PorcelainConvolverModel(sample_rate)
    dc_ext = InfrasonicDcBlockerModel(sample_rate)
    sub_ext = SubBassSineModel(sample_rate)

    max_extreme_peak = 0.0
    extreme_finite = True
    extreme_no_denorm = True
    extreme_bounded = True

    conv_ext.configure(int(eff_extreme['param_porcelain_model']), float(eff_extreme['param_porcelain_size']))

    for _ in range(10000): # 10,000 samples of continuous extreme overdrive
        osc_s, va, y = osc.step(
            pressure=eff_extreme['param_pressure'],
            tension=eff_extreme['param_tension'],
            aperture=eff_extreme['param_aperture'],
            flutter=eff_extreme['param_flutter'],
            frequency_hz=18.0 # Infrasonic rumble
        )
        t_s = turb.step(va, y)
        if random.random() < 0.05:
            bubble.trigger_bubble(1.5, 0.8 * eff_extreme['param_moisture'], eff_extreme['param_viscosity'])
        b_s = bubble.step()
        d_s = droplet.step(eff_extreme['param_droplet_rate'], eff_extreme['param_moisture'], va)
        sub_s = sub_ext.step(18.0, eff_extreme['param_sub_level'])

        raw_in = (osc_s + t_s + b_s + d_s + sub_s) * 2.0
        wg_s = wg_ext.step(raw_in, eff_extreme['param_cleft_damping'])
        c_s = conv_ext.process(wg_s, eff_extreme['param_porcelain_mix'])
        dc_s = dc_ext.step(c_s)
        sat_s = HermiteSaturatorModel.process(dc_s, eff_extreme['param_drive'])

        gain_lin = math.pow(10.0, eff_extreme['param_master_gain'] / 20.0) # +12 dB = 3.98x
        out_limited = limiter.process(sat_s * gain_lin)

        if not BitwiseNumerics.is_finite_bitwise(out_limited):
            extreme_finite = False
        if BitwiseNumerics.is_denormal(out_limited):
            extreme_no_denorm = False
        if abs(out_limited) > 1.000001:
            extreme_bounded = False
        if abs(out_limited) > max_extreme_peak:
            max_extreme_peak = abs(out_limited)

    passed_02 = extreme_finite and extreme_no_denorm and extreme_bounded and max_extreme_peak <= 1.000000
    dt_02 = (time.perf_counter() - t0) * 1000.0
    if passed_02:
        ctx.record_pass("ADV02", "F24+F15", 5, "ExtremeMacroCouplingOverload", dt_02, {"max_peak": max_extreme_peak})
    else:
        ctx.record_fail("ADV02", "F24+F15", 5, "ExtremeMacroCouplingOverload",
                        f"Extreme overload violated ceiling: peak={max_extreme_peak}", dt_02)

    # --------------------------------------------------------------------------
    # ADV-03: Peak Limiter Super-Overload Stress (Up to +60 dBFS & Infs/NaNs)
    # --------------------------------------------------------------------------
    t0 = time.perf_counter()
    lim_ok = True
    # Test sweep from -100.0 to +100.0
    for v in [-1000.0, -100.0, -10.0, -2.0, -1.0, -0.85, 0.0, 0.85, 1.0, 2.0, 10.0, 100.0, 1000.0]:
        out = limiter.process(v)
        if abs(out) > 1.000001:
            lim_ok = False
        if v >= 1.0 and abs(out - 1.0) > 1.0e-5:
            lim_ok = False
        if v <= -1.0 and abs(out - (-1.0)) > 1.0e-5:
            lim_ok = False

    # Test non-finite input flushing
    out_nan = limiter.process(float('nan'))
    out_inf = limiter.process(float('inf'))
    out_ninf = limiter.process(float('-inf'))
    if out_nan != 0.0 or out_inf != 0.0 or out_ninf != 0.0:
        lim_ok = False

    dt_03 = (time.perf_counter() - t0) * 1000.0
    if lim_ok:
        ctx.record_pass("ADV03", "F15", 5, "PeakLimiterSuperOverloadStress", dt_03)
    else:
        ctx.record_fail("ADV03", "F15", 5, "PeakLimiterSuperOverloadStress", "Peak limiter failed overload assertions", dt_03)

    # --------------------------------------------------------------------------
    # ADV-04: Continuous Decay to Silence (Zero Denormals / Subnormals)
    # --------------------------------------------------------------------------
    t0 = time.perf_counter()
    wg_decay = CleftWaveguideModel(sample_rate)
    dc_decay = InfrasonicDcBlockerModel(sample_rate)
    # Inject initial impulse
    s = wg_decay.step(1.0, 0.5)
    s = dc_decay.step(s)

    decay_denormal_free = True
    decay_samples = []
    for _ in range(50000): # ~1 second of pure decay
        out = wg_decay.step(0.0, 0.5)
        out = dc_decay.step(out)
        if BitwiseNumerics.is_denormal(out):
            decay_denormal_free = False
        decay_samples.append(out)

    final_val = abs(decay_samples[-1])
    passed_04 = decay_denormal_free and final_val < 1.0e-5
    dt_04 = (time.perf_counter() - t0) * 1000.0
    if passed_04:
        ctx.record_pass("ADV04", "F22", 5, "ContinuousDecayZeroDenormals", dt_04, {"final_val": final_val})
    else:
        ctx.record_fail("ADV04", "F22", 5, "ContinuousDecayZeroDenormals",
                        f"Decay generated denormals or failed to silence: denormal_free={decay_denormal_free}, final={final_val}", dt_04)

    # --------------------------------------------------------------------------
    # ADV-05: Standalone Binary Execution & Process Health Check
    # --------------------------------------------------------------------------
    t0 = time.perf_counter()
    exe_path = os.path.abspath(os.path.join(
        os.path.dirname(__file__), "..", "..", "build", "PPF42_Dynamics_artefacts", "Release", "Standalone", "PPF-42_Dynamics_Standalone.exe"
    ))
    exe_exists = os.path.exists(exe_path)
    exe_ran_cleanly = False
    proc_diagnostics = {}

    if exe_exists:
        try:
            # Launch standalone in non-blocking manner, verify it runs without crashing for 1.5 seconds, then terminate
            proc = subprocess.Popen([exe_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            time.sleep(1.5)
            poll = proc.poll()
            if poll is None:
                # Process is healthy and running actively
                exe_ran_cleanly = True
                proc.terminate()
                try:
                    proc.wait(timeout=1.0)
                except subprocess.TimeoutExpired:
                    proc.kill()
                proc_diagnostics["status"] = "RUNNING_AND_STABLE"
            else:
                proc_diagnostics["status"] = f"EXITED_PREMATURELY_CODE_{poll}"
        except Exception as e:
            proc_diagnostics["exception"] = str(e)

    dt_05 = (time.perf_counter() - t0) * 1000.0
    if exe_exists and exe_ran_cleanly:
        ctx.record_pass("ADV05", "Standalone", 5, "StandaloneProcessExecutionHealth", dt_05, proc_diagnostics)
    else:
        ctx.record_fail("ADV05", "Standalone", 5, "StandaloneProcessExecutionHealth",
                        f"Standalone binary test failed: exists={exe_exists}, ran_cleanly={exe_ran_cleanly}", dt_05, proc_diagnostics)

    # --------------------------------------------------------------------------
    # ADV-06: Telemetry Ring Buffer SPSC Drop & Saturation
    # --------------------------------------------------------------------------
    t0 = time.perf_counter()
    ring = TelemetryRingBuffer()
    pushed = 0
    dropped = 0
    for i in range(1000):
        frame = VisualizerFrame(aperture=float(i), colonic_pressure=0.7)
        if ring.push(frame):
            pushed += 1
        else:
            dropped += 1

    popped = 0
    while ring.pop() is not None:
        popped += 1

    passed_06 = (pushed > 0) and (dropped > 0) and (popped > 0)
    dt_06 = (time.perf_counter() - t0) * 1000.0
    if passed_06:
        ctx.record_pass("ADV06", "F26", 5, "TelemetryRingBufferSaturationDrops", dt_06,
                        {"pushed": pushed, "dropped": dropped, "popped": popped})
    else:
        ctx.record_fail("ADV06", "F26", 5, "TelemetryRingBufferSaturationDrops",
                        f"Ring buffer saturation drop failed: pushed={pushed}, dropped={dropped}", dt_06)

    # --------------------------------------------------------------------------
    # ADV-07: All 10 Factory Presets APVTS Integrity & Acoustic Displacement
    # --------------------------------------------------------------------------
    t0 = time.perf_counter()
    all_presets_valid = True
    preset_displacements = []
    param_map = {p['id']: p for p in ApvtsRegistry.PARAMETERS}

    for p_def in PresetRegistry.PRESETS:
        name = p_def["name"]
        params = p_def["params"]

        # Validate all 22 APVTS parameters exist and are in valid range
        for param_id, val in params.items():
            if param_id not in param_map:
                all_presets_valid = False
            else:
                meta = param_map[param_id]
                if val < meta["min"] or val > meta["max"]:
                    all_presets_valid = False

        # Run 2000 samples to verify acoustic displacement across exciter + aero turbulence + sub bass
        osc_p = SphincterOscillatorModel(sample_rate)
        turb_p = ReynoldsTurbulenceModel(sample_rate)
        sub_p = SubBassSineModel(sample_rate)
        outs = []
        for _ in range(2000):
            s, va, y = osc_p.step(
                pressure=params["param_pressure"],
                tension=params["param_tension"],
                aperture=params["param_aperture"],
                flutter=params["param_flutter"],
                frequency_hz=110.0
            )
            t = turb_p.step(va, y)
            sb = sub_p.step(110.0, params.get("param_sub_level", -12.0))
            outs.append(s + t + sb)

        rms = math.sqrt(sum(x*x for x in outs) / len(outs))
        preset_displacements.append(rms)
        if rms < 0.001:
            all_presets_valid = False

    dt_07 = (time.perf_counter() - t0) * 1000.0
    if all_presets_valid:
        ctx.record_pass("ADV07", "F25", 5, "All10PresetsIntegrityAudit", dt_07,
                        {"min_rms": min(preset_displacements), "max_rms": max(preset_displacements)})
    else:
        ctx.record_fail("ADV07", "F25", 5, "All10PresetsIntegrityAudit",
                        "One or more factory presets invalid or silent", dt_07)
