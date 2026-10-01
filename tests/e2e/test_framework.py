"""
PPF-42 DYNAMICS E2E Testing Framework
Shared Testing Harness, Mathematical DSP Oracles, APVTS Specifications, and Assertion Helpers.
Authoritative Reference: ORIGINAL_REQUEST.md, PROJECT.md, PPF42_DYNAMICS_SPEC.md, spec_inventory.md
"""

import math
import struct
import random
import time
from dataclasses import dataclass, field
from typing import List, Dict, Tuple, Optional, Callable, Any

# ==============================================================================
# 1. TEST HARNESS & DATA STRUCTURES
# ==============================================================================

@dataclass
class TestResult:
    test_id: str
    feature_id: str
    tier: int
    name: str
    passed: bool
    message: str = ""
    duration_ms: float = 0.0
    diagnostics: Dict[str, Any] = field(default_factory=dict)


class TestRunnerContext:
    def __init__(self):
        self.results: List[TestResult] = []
        self.start_time: float = 0.0

    def record_pass(self, test_id: str, feature_id: str, tier: int, name: str, duration_ms: float = 0.0, diagnostics: Optional[Dict[str, Any]] = None):
        self.results.append(TestResult(
            test_id=test_id,
            feature_id=feature_id,
            tier=tier,
            name=name,
            passed=True,
            message="PASS",
            duration_ms=duration_ms,
            diagnostics=diagnostics or {}
        ))

    def record_fail(self, test_id: str, feature_id: str, tier: int, name: str, message: str, duration_ms: float = 0.0, diagnostics: Optional[Dict[str, Any]] = None):
        self.results.append(TestResult(
            test_id=test_id,
            feature_id=feature_id,
            tier=tier,
            name=name,
            passed=False,
            message=message,
            duration_ms=duration_ms,
            diagnostics=diagnostics or {}
        ))


# ==============================================================================
# 2. IEEE 754 BITWISE NUMERICS & SUB-NORMAL FLUSHING
# ==============================================================================

class BitwiseNumerics:
    """Bitwise IEEE-754 finite check and subnormal detector matching C++ DspMath.h"""

    @staticmethod
    def float_to_bits(val: float) -> int:
        packed = struct.pack('>f', float(val))
        return struct.unpack('>I', packed)[0]

    @staticmethod
    def bits_to_float(bits: int) -> float:
        packed = struct.pack('>I', bits & 0xFFFFFFFF)
        return struct.unpack('>f', packed)[0]

    @classmethod
    def is_finite_bitwise(cls, val: float) -> bool:
        """IEEE 754: Exponent field [30:23] all-ones (0x7F800000) indicates Inf or NaN"""
        try:
            bits = cls.float_to_bits(val)
            return (bits & 0x7F800000) != 0x7F800000
        except Exception:
            return False

    @classmethod
    def is_denormal(cls, val: float) -> bool:
        """Subnormal/denormal: exponent is 0, mantissa is non-zero (non-zero magnitude < 1.175494e-38)"""
        if val == 0.0:
            return False
        bits = cls.float_to_bits(val)
        exponent = (bits & 0x7F800000) >> 23
        mantissa = bits & 0x007FFFFF
        return exponent == 0 and mantissa != 0

    @classmethod
    def flush_denormal(cls, val: float) -> float:
        if not cls.is_finite_bitwise(val):
            return 0.0
        return 0.0 if abs(val) < 1.0e-15 else float(val)


# ==============================================================================
# 3. MATHEMATICAL DSP ORACLES
# ==============================================================================

class SphincterOscillatorModel:
    """
    Physical modeling oracle for Fletcher/Adachi non-linear relaxation oscillator:
    m * d2y/dt2 + r * dy/dt + k * (y - y0) = F_net
    F_net = DeltaP(t) - P_Bernoulli(t) + F_contact
    """

    def __init__(self, sample_rate: float = 48000.0):
        self.sample_rate = sample_rate
        self.reset()

    def reset(self):
        self.y = 0.5            # Aperture displacement (m)
        self.v = 0.0            # Velocity dy/dt
        self.air_velocity = 0.0 # v_air through orifice
        self.upstream_p = 0.0   # Upstream pressure head

    def step(self, pressure: float, tension: float, aperture: float, flutter: float, frequency_hz: float) -> Tuple[float, float, float]:
        """
        Processes 1 sample.
        Returns: (output_sample, air_velocity, aperture_displacement)
        """
        dt = 1.0 / self.sample_rate
        f0 = max(18.0, min(350.0, float(frequency_hz)))
        omega0 = 2.0 * math.pi * f0
        k0 = omega0 * omega0
        damping_r = 2.0 * 0.12 * omega0

        y0 = max(0.01, min(0.95, float(aperture)))
        colonic_head = max(0.0, float(pressure)) * 2.5

        # Update upstream pressure
        self.upstream_p = max(0.0, min(5.0, self.upstream_p + dt * (colonic_head - self.air_velocity * max(0.0, self.y))))
        self.upstream_p = BitwiseNumerics.flush_denormal(self.upstream_p)

        def calc_acc(y_val: float, v_val: float, p_head: float) -> float:
            contact_k = (1.0 + 0.05 / (y_val * y_val + 1e-4)) if y_val < 0.10 else 1.0
            spring = -k0 * contact_k * (y_val - y0)
            damp = -damping_r * v_val
            v_air = math.sqrt(max(0.0, 2.0 * p_head)) if p_head > 0.0 else 0.0
            gap = max(0.02, y_val)
            p_bernoulli = 0.5 * 1.2 * (v_air * v_air) * (y0 / gap)
            driving_force = (p_head - p_bernoulli) * 1.2
            flut = flutter * 0.2 * math.sin(3.0 * omega0 * y_val)
            return spring + damp + driving_force + flut

        # Midpoint / Symplectic Euler integration
        a1 = calc_acc(self.y, self.v, self.upstream_p)
        y_mid = max(0.0, min(2.0, self.y + 0.5 * dt * self.v))
        v_mid = max(-50.0, min(50.0, self.v + 0.5 * dt * a1))

        a2 = calc_acc(y_mid, v_mid, self.upstream_p)
        self.y = self.y + dt * v_mid
        self.v = max(-50.0, min(50.0, self.v + dt * a2))

        # Hard restitution boundary at y = 0
        if self.y <= 0.0:
            self.y = 0.0
            if self.v < 0.0:
                self.v = -0.35 * self.v # Coefficient of restitution

        self.y = BitwiseNumerics.flush_denormal(self.y)
        self.v = BitwiseNumerics.flush_denormal(self.v)

        if self.y <= 0.0:
            self.air_velocity = 0.0
        else:
            self.air_velocity = math.sqrt(max(0.0, 2.0 * self.upstream_p)) * self.y

        # Radiated acoustic pressure wave
        acoustic_out = (self.y - y0) * 1.5
        return (acoustic_out, self.air_velocity, self.y)


class ReynoldsTurbulenceModel:
    """
    Aeroacoustic jet turbulence noise model:
    N_turb = eta_pink * |v_air|^2.5 * A_effective
    Kellet 3-pole pinking filter.
    """

    def __init__(self, sample_rate: float = 48000.0, seed: int = 12345):
        self.sample_rate = sample_rate
        self.rng = random.Random(seed)
        self.b0 = 0.0
        self.b1 = 0.0
        self.b2 = 0.0

    def reset(self):
        self.b0 = 0.0
        self.b1 = 0.0
        self.b2 = 0.0

    def step(self, air_velocity: float, aperture: float) -> float:
        if aperture <= 0.0 or air_velocity <= 0.0:
            return 0.0

        white = self.rng.uniform(-1.0, 1.0)
        self.b0 = 0.99765 * self.b0 + white * 0.0990460
        self.b1 = 0.96300 * self.b1 + white * 0.2965164
        self.b2 = 0.57000 * self.b2 + white * 1.0526913
        pink = self.b0 + self.b1 + self.b2 + white * 0.1848

        turb_power = math.pow(max(0.0, air_velocity), 2.5)
        area = max(0.0, aperture)
        out = pink * turb_power * area * 0.08
        return max(-1.0, min(1.0, BitwiseNumerics.flush_denormal(out)))


class MinnaertBubbleModel:
    """
    Minnaert micro-bubble resonance model:
    f_b = 1 / (2 * pi * R) * sqrt(3 * gamma * P0 / rho)
    For R in [0.5, 4.0] mm -> f_b in [800 Hz, 6500 Hz].
    Damped sinusoidal pulses. Max 16 concurrent voices.
    """

    @staticmethod
    def calculate_frequency(radius_mm: float) -> float:
        r_m = max(0.0005, min(0.0040, radius_mm * 0.001))
        gamma = 1.4
        p0 = 101325.0
        rho = 1000.0
        freq = (1.0 / (2.0 * math.pi * r_m)) * math.sqrt(3.0 * gamma * p0 / rho)
        return max(800.0, min(6500.0, freq))

    def __init__(self, sample_rate: float = 48000.0):
        self.sample_rate = sample_rate
        self.voices = [] # list of dicts: {phase, inc, env, decay}
        self.max_voices = 16

    def reset(self):
        self.voices.clear()

    def trigger_bubble(self, radius_mm: float, amplitude: float = 0.5, viscosity: float = 0.2):
        freq = self.calculate_frequency(radius_mm)
        phase_inc = 2.0 * math.pi * freq / self.sample_rate
        decay = 1.0 - (0.01 + 0.05 * viscosity) # viscous damping

        if len(self.voices) >= self.max_voices:
            # Oldest-voice stealing
            self.voices.pop(0)

        self.voices.append({
            'phase': 0.0,
            'inc': phase_inc,
            'env': amplitude,
            'decay': max(0.90, min(0.999, decay))
        })

    def step(self) -> float:
        total = 0.0
        active = []
        for v in self.voices:
            if v['env'] > 1e-4:
                sample = math.sin(v['phase']) * v['env']
                total += sample
                v['phase'] = (v['phase'] + v['inc']) % (2.0 * math.pi)
                v['env'] *= v['decay']
                active.append(v)
        self.voices = active
        return BitwiseNumerics.flush_denormal(total)


class PoissonDropletModel:
    """
    Poisson droplet splatter engine using exact inverse-CDF sampling:
    dt = -ln(1 - U) / lambda * fs
    Biquad resonant pop bandpass filter (1.2 kHz - 4.8 kHz).
    """

    def __init__(self, sample_rate: float = 48000.0, seed: int = 54321):
        self.sample_rate = sample_rate
        self.rng = random.Random(seed)
        self.countdown = 100
        self.pop_env = 0.0
        self.pop_phase = 0.0
        self.pop_inc = 0.0

    def reset(self):
        self.countdown = 100
        self.pop_env = 0.0
        self.pop_phase = 0.0
        self.pop_inc = 0.0

    def step(self, droplet_rate: float, moisture: float, air_velocity: float) -> float:
        if droplet_rate <= 0.0 or moisture <= 0.0 or air_velocity <= 0.0:
            return 0.0

        lambda_val = max(0.1, droplet_rate * moisture * air_velocity * 40.0)
        self.countdown -= 1

        if self.countdown <= 0:
            u = max(1e-6, min(1.0 - 1e-6, self.rng.random()))
            interval_samples = int(-math.log(1.0 - u) / lambda_val * self.sample_rate)
            self.countdown = max(1, interval_samples)

            # Trigger new pop
            pop_freq = self.rng.uniform(1200.0, 4800.0)
            self.pop_inc = 2.0 * math.pi * pop_freq / self.sample_rate
            self.pop_env = 0.6
            self.pop_phase = 0.0

        out = 0.0
        if self.pop_env > 1e-4:
            out = math.sin(self.pop_phase) * self.pop_env
            self.pop_phase = (self.pop_phase + self.pop_inc) % (2.0 * math.pi)
            self.pop_env *= 0.94 # fast pop decay

        return BitwiseNumerics.flush_denormal(out)


class CleftWaveguideModel:
    """
    Intergluteal cleft boundary waveguide comb filter:
    y[n] = x[n] + g * x[n - D] - a_damp * y[n - 1]
    D = round(0.0012 * fs) (~1.2 ms), g <= 0.95.
    """

    def __init__(self, sample_rate: float = 48000.0):
        self.sample_rate = sample_rate
        self.delay_len = max(4, int(round(0.0012 * sample_rate)))
        self.buffer = [0.0] * (self.delay_len * 2)
        self.write_idx = 0
        self.prev_out = 0.0

    def reset(self):
        self.buffer = [0.0] * len(self.buffer)
        self.write_idx = 0
        self.prev_out = 0.0

    def step(self, x: float, damping: float) -> float:
        read_idx = (self.write_idx - self.delay_len) % len(self.buffer)
        delayed = self.buffer[read_idx]

        g = 0.65 * (1.0 - 0.5 * max(0.0, min(1.0, damping)))
        a_damp = 0.25 * max(0.0, min(1.0, damping))

        y = x + g * delayed - a_damp * self.prev_out
        self.prev_out = BitwiseNumerics.flush_denormal(y)

        self.buffer[self.write_idx] = x
        self.write_idx = (self.write_idx + 1) % len(self.buffer)

        return y


class PorcelainConvolverModel:
    """
    Porcelain cavity convolver oracle simulating 4 ceramic boundary models:
    0: Dry Chamber, 1: Ceramic Bowl, 2: Water Coupled, 3: Tiled Enclosure.
    """

    MODELS = {
        0: "Dry Chamber",
        1: "Ceramic Bowl - Standard",
        2: "Ceramic Bowl - Water Coupled",
        3: "Tiled Enclosure"
    }

    def __init__(self, sample_rate: float = 48000.0):
        self.sample_rate = sample_rate
        self.biquads = []

    def configure(self, model: int, size: float):
        self.biquads.clear()
        scale = max(0.5, min(2.0, size))

        if model == 1:
            # Resonant modes at 420 Hz and 1180 Hz shifted by 1/scale
            self.biquads.append({'f': (420.0 / scale), 'q': 8.0, 'g': 0.6, 'x1': 0.0, 'x2': 0.0, 'y1': 0.0, 'y2': 0.0})
            self.biquads.append({'f': (1180.0 / scale), 'q': 12.0, 'g': 0.4, 'x1': 0.0, 'x2': 0.0, 'y1': 0.0, 'y2': 0.0})
        elif model == 2:
            # Low pass loading around 600 Hz
            self.biquads.append({'f': (550.0 / scale), 'q': 3.0, 'g': 0.7, 'x1': 0.0, 'x2': 0.0, 'y1': 0.0, 'y2': 0.0})
        elif model == 3:
            # Tiled enclosure multiple reflections
            self.biquads.append({'f': (850.0 / scale), 'q': 5.0, 'g': 0.5, 'x1': 0.0, 'x2': 0.0, 'y1': 0.0, 'y2': 0.0})
            self.biquads.append({'f': (2400.0 / scale), 'q': 6.0, 'g': 0.3, 'x1': 0.0, 'x2': 0.0, 'y1': 0.0, 'y2': 0.0})

    def process(self, x: float, mix: float) -> float:
        if mix <= 0.0 or not self.biquads:
            return x

        wet = 0.0
        for b in self.biquads:
            # Simple resonator filter step
            w0 = 2.0 * math.pi * b['f'] / self.sample_rate
            alpha = math.sin(w0) / (2.0 * b['q'])
            b0 = alpha
            a0 = 1.0 + alpha
            a1 = -2.0 * math.cos(w0)
            a2 = 1.0 - alpha

            y = (b0 * x - a1 * b['y1'] - a2 * b['y2']) / a0
            b['y2'] = b['y1']
            b['y1'] = BitwiseNumerics.flush_denormal(y)
            wet += y * b['g']

        return (1.0 - mix) * x + mix * wet


class InfrasonicDcBlockerModel:
    """
    1-pole DC blocker with cutoff at 15 Hz:
    y[n] = x[n] - x[n-1] + R * y[n-1]
    R = clamp(1.0 - 2*pi*15/fs, 0.0, 0.99995)
    Strips static DC while preserving 18 Hz fundamental (< 1.5 dB attenuation).
    """

    def __init__(self, sample_rate: float = 48000.0):
        self.sample_rate = sample_rate
        self.x_prev = 0.0
        self.y_prev = 0.0
        self.update_coeff()

    def reset(self):
        self.x_prev = 0.0
        self.y_prev = 0.0

    def update_coeff(self):
        self.r = max(0.0, min(0.99995, 1.0 - (2.0 * math.pi * 15.0 / self.sample_rate)))

    def step(self, x: float) -> float:
        y = x - self.x_prev + self.r * self.y_prev
        self.x_prev = x
        self.y_prev = BitwiseNumerics.flush_denormal(y)
        return y


class SubBassSineModel:
    """Dedicated sub-bass sine oscillator phase-locked to fundamental frequency."""

    def __init__(self, sample_rate: float = 48000.0):
        self.sample_rate = sample_rate
        self.phase = 0.0

    def step(self, freq_hz: float, sub_level_db: float) -> float:
        if sub_level_db <= -59.0:
            return 0.0
        amp = math.pow(10.0, sub_level_db / 20.0)
        f0 = max(18.0, min(350.0, freq_hz))
        out = math.sin(self.phase) * amp
        inc = 2.0 * math.pi * f0 / self.sample_rate
        self.phase = (self.phase + inc) % (2.0 * math.pi)
        return BitwiseNumerics.flush_denormal(out)


class HermiteSaturatorModel:
    """
    Harmonic saturation drive with C1 cubic Hermite soft knee:
    For |x| <= 0.72: y = x (linear region)
    For 0.72 < |x| < 1.05: smooth cubic Hermite knee
    For |x| >= 1.05: hard ceiling clamp at 1.05
    """

    K_KNEE = 0.72
    M_CEILING = 1.05

    @classmethod
    def process(cls, x: float, drive: float) -> float:
        gain = 1.0 + 3.0 * max(0.0, min(1.0, drive))
        driven = x * gain

        abs_x = abs(driven)
        sgn = 1.0 if driven >= 0.0 else -1.0

        if abs_x <= cls.K_KNEE:
            return driven
        elif abs_x >= cls.M_CEILING:
            return sgn * cls.M_CEILING
        else:
            u = (abs_x - cls.K_KNEE) / (cls.M_CEILING - cls.K_KNEE)
            poly = u + u * u - u * u * u
            return sgn * (cls.K_KNEE + (cls.M_CEILING - cls.K_KNEE) * poly)


class TruePeakLimiterModel:
    """True-peak brickwall peak limiter strictly enforcing ceiling <= 1.0000 (0 dBFS)."""

    @staticmethod
    def process(x: float, master_gain_db: float = 0.0) -> float:
        gain = math.pow(10.0, master_gain_db / 20.0)
        signal = x * gain

        if not BitwiseNumerics.is_finite_bitwise(signal):
            return 0.0

        abs_s = abs(signal)
        sgn = 1.0 if signal >= 0.0 else -1.0

        if abs_s <= 0.85:
            return signal
        elif abs_s >= 1.0:
            return sgn * 1.0000
        else:
            # Soft knee towards 1.0
            u = (abs_s - 0.85) / 0.15
            poly = u * (1.0 + u * (1.0 - u))
            return sgn * min(1.0000, 0.85 + 0.15 * poly)


# ==============================================================================
# 4. APVTS REGISTRY & PRESET DEFINITIONS
# ==============================================================================

class ApvtsRegistry:
    """Authoritative registry of all 22 APVTS parameters."""

    PARAMETERS = [
        {"id": "param_pressure", "name": "Colonic Drive", "min": 0.0, "max": 1.0, "default": 0.70, "skew": 1.0},
        {"id": "param_tension", "name": "Tissue Elasticity", "min": 0.0, "max": 1.0, "default": 0.50, "skew": 1.0},
        {"id": "param_aperture", "name": "Resting Gap", "min": 0.0, "max": 1.0, "default": 0.30, "skew": 1.0},
        {"id": "param_flutter", "name": "Chaos / Asymmetry", "min": 0.0, "max": 1.0, "default": 0.20, "skew": 1.0},
        {"id": "param_viscosity", "name": "Fluid Viscosity", "min": 0.0, "max": 1.0, "default": 0.25, "skew": 1.0},
        {"id": "param_moisture", "name": "Moisture Level", "min": 0.0, "max": 1.0, "default": 0.20, "skew": 1.0},
        {"id": "param_droplet_rate", "name": "Splatter Density", "min": 0.0, "max": 1.0, "default": 0.30, "skew": 1.0},
        {"id": "param_cleft_damping", "name": "Boundary Damping", "min": 0.0, "max": 1.0, "default": 0.50, "skew": 1.0},
        {"id": "param_porcelain_mix", "name": "Porcelain IR Mix", "min": 0.0, "max": 1.0, "default": 0.35, "skew": 1.0},
        {"id": "param_porcelain_size", "name": "Cavity Volume", "min": 0.5, "max": 2.0, "default": 1.00, "skew": 1.0},
        {"id": "param_porcelain_model", "name": "Resonant Model", "min": 0, "max": 3, "default": 1, "skew": 1.0},
        {"id": "param_voice_mode", "name": "Voice Mode", "min": 0, "max": 2, "default": 0, "skew": 1.0},
        {"id": "param_glide_time", "name": "Portamento", "min": 0.0, "max": 500.0, "default": 50.0, "skew": 0.35},
        {"id": "param_sub_level", "name": "Sub Reinforcement", "min": -60.0, "max": 6.0, "default": -12.0, "skew": 2.0},
        {"id": "param_drive", "name": "Saturation Drive", "min": 0.0, "max": 1.0, "default": 0.15, "skew": 1.0},
        {"id": "param_master_gain", "name": "Master Output", "min": -60.0, "max": 12.0, "default": 0.00, "skew": 2.0},
        {"id": "macro_squeeze", "name": "Gut Squeeze", "min": 0.0, "max": 1.0, "default": 0.50, "skew": 1.0},
        {"id": "macro_moisture", "name": "Dietary Moisture", "min": 0.0, "max": 1.0, "default": 0.20, "skew": 1.0},
        {"id": "param_env_attack", "name": "Pressure Attack", "min": 0.1, "max": 200.0, "default": 5.0, "skew": 0.30},
        {"id": "param_env_decay", "name": "Pressure Decay", "min": 10.0, "max": 2000.0, "default": 350.0, "skew": 0.35},
        {"id": "param_env_sustain", "name": "Pressure Sustain", "min": 0.0, "max": 1.0, "default": 0.60, "skew": 1.0},
        {"id": "param_env_release", "name": "Pressure Release", "min": 10.0, "max": 2000.0, "default": 120.0, "skew": 0.35},
    ]

    PARAM_MAP = {p["id"]: p for p in PARAMETERS}

    @classmethod
    def clamp(cls, param_id: str, value: float) -> float:
        meta = cls.PARAM_MAP.get(param_id)
        if not meta:
            return value
        return max(float(meta["min"]), min(float(meta["max"]), float(value)))


class MacroCalculator:
    """Non-linear macro transfer curves from Blueprint Section 5."""

    @staticmethod
    def calculate_effective_parameters(params: Dict[str, float], macro_squeeze: float, macro_moisture: float) -> Dict[str, float]:
        ms = max(0.0, min(1.0, macro_squeeze))
        mm = max(0.0, min(1.0, macro_moisture))

        eff = dict(params)
        p_base = params.get("param_pressure", 0.70)
        t_base = params.get("param_tension", 0.50)
        f_base = params.get("param_flutter", 0.20)
        v_base = params.get("param_viscosity", 0.25)
        m_base = params.get("param_moisture", 0.20)
        d_base = params.get("param_droplet_rate", 0.30)

        eff["param_pressure"] = max(0.0, min(1.0, p_base * (0.5 + 0.8 * ms)))
        eff["param_tension"] = max(0.0, min(1.0, t_base * (0.7 + 0.6 * ms)))
        eff["param_flutter"] = max(0.0, min(1.0, f_base + 0.4 * (ms * ms)))

        eff["param_viscosity"] = max(0.0, min(1.0, v_base * (0.4 + 1.2 * mm)))
        eff["param_moisture"] = max(0.0, min(1.0, m_base * (0.2 + 1.6 * mm)))
        eff["param_droplet_rate"] = max(0.0, min(1.0, d_base * math.pow(mm, 1.5)))

        return eff


class PresetRegistry:
    """Authoritative dictionary of the 10 anatomical factory presets."""

    PRESETS = [
        {
            "index": 0,
            "name": "01 - Clean Continental Purr",
            "params": {
                "param_pressure": 0.65, "param_tension": 0.45, "param_aperture": 0.35, "param_flutter": 0.10,
                "param_viscosity": 0.10, "param_moisture": 0.05, "param_droplet_rate": 0.05, "param_cleft_damping": 0.60,
                "param_porcelain_mix": 0.20, "param_porcelain_size": 1.00, "param_porcelain_model": 1, "param_voice_mode": 0,
                "param_glide_time": 40.0, "param_sub_level": -9.0, "param_drive": 0.10, "param_master_gain": 0.0,
                "macro_squeeze": 0.40, "macro_moisture": 0.15,
                "param_env_attack": 10.0, "param_env_decay": 400.0, "param_env_sustain": 0.50, "param_env_release": 100.0
            }
        },
        {
            "index": 1,
            "name": "02 - High-Tension Squeaker",
            "params": {
                "param_pressure": 0.85, "param_tension": 0.92, "param_aperture": 0.08, "param_flutter": 0.05,
                "param_viscosity": 0.05, "param_moisture": 0.00, "param_droplet_rate": 0.00, "param_cleft_damping": 0.30,
                "param_porcelain_mix": 0.15, "param_porcelain_size": 0.70, "param_porcelain_model": 0, "param_voice_mode": 0,
                "param_glide_time": 25.0, "param_sub_level": -24.0, "param_drive": 0.30, "param_master_gain": -1.0,
                "macro_squeeze": 0.85, "macro_moisture": 0.00,
                "param_env_attack": 2.0, "param_env_decay": 200.0, "param_env_sustain": 0.70, "param_env_release": 50.0
            }
        },
        {
            "index": 2,
            "name": "03 - Viscous Multiphase Splatter",
            "params": {
                "param_pressure": 0.78, "param_tension": 0.40, "param_aperture": 0.45, "param_flutter": 0.35,
                "param_viscosity": 0.85, "param_moisture": 0.80, "param_droplet_rate": 0.75, "param_cleft_damping": 0.45,
                "param_porcelain_mix": 0.40, "param_porcelain_size": 1.10, "param_porcelain_model": 2, "param_voice_mode": 0,
                "param_glide_time": 60.0, "param_sub_level": -6.0, "param_drive": 0.25, "param_master_gain": 0.0,
                "macro_squeeze": 0.60, "macro_moisture": 0.85,
                "param_env_attack": 5.0, "param_env_decay": 500.0, "param_env_sustain": 0.40, "param_env_release": 150.0
            }
        },
        {
            "index": 3,
            "name": "04 - Visceral Sub-Rumble (18 Hz)",
            "params": {
                "param_pressure": 0.90, "param_tension": 0.12, "param_aperture": 0.60, "param_flutter": 0.25,
                "param_viscosity": 0.30, "param_moisture": 0.20, "param_droplet_rate": 0.15, "param_cleft_damping": 0.80,
                "param_porcelain_mix": 0.50, "param_porcelain_size": 1.60, "param_porcelain_model": 1, "param_voice_mode": 0,
                "param_glide_time": 80.0, "param_sub_level": 3.0, "param_drive": 0.45, "param_master_gain": 1.0,
                "macro_squeeze": 0.70, "macro_moisture": 0.30,
                "param_env_attack": 20.0, "param_env_decay": 800.0, "param_env_sustain": 0.80, "param_env_release": 300.0
            }
        },
        {
            "index": 4,
            "name": "05 - Flutter-Tongue Stutter",
            "params": {
                "param_pressure": 0.75, "param_tension": 0.50, "param_aperture": 0.30, "param_flutter": 0.95,
                "param_viscosity": 0.20, "param_moisture": 0.15, "param_droplet_rate": 0.20, "param_cleft_damping": 0.40,
                "param_porcelain_mix": 0.25, "param_porcelain_size": 1.00, "param_porcelain_model": 0, "param_voice_mode": 0,
                "param_glide_time": 30.0, "param_sub_level": -12.0, "param_drive": 0.20, "param_master_gain": 0.0,
                "macro_squeeze": 0.80, "macro_moisture": 0.20,
                "param_env_attack": 5.0, "param_env_decay": 450.0, "param_env_sustain": 0.60, "param_env_release": 80.0
            }
        },
        {
            "index": 5,
            "name": "06 - Wet Porcelain Slam",
            "params": {
                "param_pressure": 0.82, "param_tension": 0.38, "param_aperture": 0.40, "param_flutter": 0.30,
                "param_viscosity": 0.70, "param_moisture": 0.75, "param_droplet_rate": 0.60, "param_cleft_damping": 0.30,
                "param_porcelain_mix": 0.75, "param_porcelain_size": 1.30, "param_porcelain_model": 1, "param_voice_mode": 0,
                "param_glide_time": 45.0, "param_sub_level": -3.0, "param_drive": 0.35, "param_master_gain": 0.0,
                "macro_squeeze": 0.65, "macro_moisture": 0.75,
                "param_env_attack": 8.0, "param_env_decay": 600.0, "param_env_sustain": 0.50, "param_env_release": 200.0
            }
        },
        {
            "index": 6,
            "name": "07 - Micro-Puff Staccato",
            "params": {
                "param_pressure": 0.60, "param_tension": 0.65, "param_aperture": 0.20, "param_flutter": 0.05,
                "param_viscosity": 0.10, "param_moisture": 0.05, "param_droplet_rate": 0.10, "param_cleft_damping": 0.70,
                "param_porcelain_mix": 0.10, "param_porcelain_size": 0.80, "param_porcelain_model": 0, "param_voice_mode": 0,
                "param_glide_time": 0.0, "param_sub_level": -18.0, "param_drive": 0.05, "param_master_gain": 2.0,
                "macro_squeeze": 0.30, "macro_moisture": 0.10,
                "param_env_attack": 0.5, "param_env_decay": 60.0, "param_env_sustain": 0.00, "param_env_release": 15.0
            }
        },
        {
            "index": 7,
            "name": "08 - Extended Gaseous Drift",
            "params": {
                "param_pressure": 0.55, "param_tension": 0.35, "param_aperture": 0.50, "param_flutter": 0.40,
                "param_viscosity": 0.15, "param_moisture": 0.10, "param_droplet_rate": 0.05, "param_cleft_damping": 0.50,
                "param_porcelain_mix": 0.30, "param_porcelain_size": 1.00, "param_porcelain_model": 3, "param_voice_mode": 0,
                "param_glide_time": 120.0, "param_sub_level": -8.0, "param_drive": 0.15, "param_master_gain": 0.0,
                "macro_squeeze": 0.35, "macro_moisture": 0.15,
                "param_env_attack": 40.0, "param_env_decay": 1800.0, "param_env_sustain": 0.75, "param_env_release": 400.0
            }
        },
        {
            "index": 8,
            "name": "09 - Unison Twin Cannons",
            "params": {
                "param_pressure": 0.85, "param_tension": 0.42, "param_aperture": 0.35, "param_flutter": 0.50,
                "param_viscosity": 0.30, "param_moisture": 0.25, "param_droplet_rate": 0.30, "param_cleft_damping": 0.40,
                "param_porcelain_mix": 0.45, "param_porcelain_size": 1.15, "param_porcelain_model": 1, "param_voice_mode": 2,
                "param_glide_time": 50.0, "param_sub_level": 0.0, "param_drive": 0.40, "param_master_gain": -2.0,
                "macro_squeeze": 0.75, "macro_moisture": 0.30,
                "param_env_attack": 10.0, "param_env_decay": 500.0, "param_env_sustain": 0.70, "param_env_release": 150.0
            }
        },
        {
            "index": 9,
            "name": "10 - The Brown Note 808",
            "params": {
                "param_pressure": 0.95, "param_tension": 0.10, "param_aperture": 0.25, "param_flutter": 0.15,
                "param_viscosity": 0.20, "param_moisture": 0.10, "param_droplet_rate": 0.10, "param_cleft_damping": 0.90,
                "param_porcelain_mix": 0.20, "param_porcelain_size": 1.50, "param_porcelain_model": 1, "param_voice_mode": 0,
                "param_glide_time": 15.0, "param_sub_level": 6.0, "param_drive": 0.55, "param_master_gain": 1.0,
                "macro_squeeze": 0.90, "macro_moisture": 0.15,
                "param_env_attack": 1.0, "param_env_decay": 1200.0, "param_env_sustain": 0.30, "param_env_release": 250.0
            }
        }
    ]


# ==============================================================================
# 5. VOICE & NOTE MANAGEMENT ORACLE
# ==============================================================================

class VoiceManagerModel:
    """Voice manager supporting Mono-Legato (with portamento), Polyphonic (8-voice with stealing), and Unison Detune."""

    def __init__(self, max_voices: int = 8, sample_rate: float = 48000.0):
        self.max_voices = max_voices
        self.sample_rate = sample_rate
        self.voices = [] # active voices: {note, vel, time_active, state, osc, freq, target_freq}
        self.note_stack = [] # for mono legato
        self.voice_mode = 0  # 0: Mono, 1: Poly, 2: Unison

    def reset(self):
        self.voices.clear()
        self.note_stack.clear()

    @staticmethod
    def note_to_freq(note: int) -> float:
        f = 440.0 * math.pow(2.0, (note - 69) / 12.0)
        return max(18.0, min(350.0, f))

    def note_on(self, note: int, velocity: int, glide_time_ms: float = 50.0):
        f_target = self.note_to_freq(note)

        if self.voice_mode == 0:
            # Mono Legato
            if not self.voices:
                self.voices.append({
                    'note': note,
                    'velocity': velocity,
                    'freq': f_target,
                    'target_freq': f_target,
                    'glide_tau': max(0.0001, glide_time_ms * 0.001),
                    'osc': SphincterOscillatorModel(self.sample_rate)
                })
            else:
                # Legato transition
                v = self.voices[0]
                v['note'] = note
                v['target_freq'] = f_target
                v['glide_tau'] = max(0.0001, glide_time_ms * 0.001)
            self.note_stack.append(note)

        elif self.voice_mode == 1:
            # Polyphonic 8-voice
            stolen = False
            if len(self.voices) >= self.max_voices:
                # Steal oldest
                self.voices.pop(0)
                stolen = True
            v = {
                'note': note,
                'velocity': velocity,
                'freq': f_target,
                'target_freq': f_target,
                'glide_tau': 0.001,
                'stolen': stolen,
                'osc': SphincterOscillatorModel(self.sample_rate)
            }
            self.voices.append(v)

        elif self.voice_mode == 2:
            # Unison Detune mode (4 detuned voices)
            self.voices.clear()
            detunes = [-0.14, -0.07, +0.07, +0.14] # semitone/cent detunes
            pans = [0.15, 0.35, 0.65, 0.85]
            for d, pan in zip(detunes, pans):
                detuned_f = max(18.0, min(350.0, f_target * math.pow(2.0, d / 12.0)))
                self.voices.append({
                    'note': note,
                    'velocity': velocity,
                    'freq': detuned_f,
                    'target_freq': detuned_f,
                    'pan': pan,
                    'osc': SphincterOscillatorModel(self.sample_rate)
                })

    def note_off(self, note: int):
        if self.voice_mode == 0:
            if note in self.note_stack:
                self.note_stack.remove(note)
            if self.note_stack:
                # Return to top of stack
                prev_note = self.note_stack[-1]
                if self.voices:
                    self.voices[0]['target_freq'] = self.note_to_freq(prev_note)
            else:
                self.voices.clear()
        elif self.voice_mode == 1:
            self.voices = [v for v in self.voices if v['note'] != note]
        elif self.voice_mode == 2:
            self.voices.clear()


# ==============================================================================
# 6. SPSC TELEMETRY BUFFER ORACLE
# ==============================================================================

@dataclass
class VisualizerFrame:
    aperture: float = 0.0
    air_velocity: float = 0.0
    colonic_pressure: float = 0.0
    bubble_activity: float = 0.0
    droplet_pops: float = 0.0
    cleft_energy: float = 0.0
    rms_l: float = 0.0
    rms_r: float = 0.0
    scope_samples: List[float] = field(default_factory=lambda: [0.0] * 128)


class TelemetryRingBuffer:
    """Lock-Free Single-Producer Single-Consumer (SPSC) ring buffer with power-of-two mask."""

    CAPACITY = 32
    MASK = CAPACITY - 1

    def __init__(self):
        self.buffer = [VisualizerFrame() for _ in range(self.CAPACITY)]
        self.write_idx = 0
        self.read_idx = 0

    def push(self, frame: VisualizerFrame) -> bool:
        if ((self.write_idx + 1) & self.MASK) != (self.read_idx & self.MASK):
            self.buffer[self.write_idx & self.MASK] = frame
            self.write_idx += 1
            return True
        else:
            # Overwrite oldest / drop to preserve non-blocking real-time guarantee
            self.buffer[self.write_idx & self.MASK] = frame
            self.write_idx += 1
            return False

    def pop(self) -> Optional[VisualizerFrame]:
        if self.read_idx != self.write_idx:
            frame = self.buffer[self.read_idx & self.MASK]
            self.read_idx += 1
            return frame
        return None

    def is_empty(self) -> bool:
        return self.read_idx == self.write_idx
