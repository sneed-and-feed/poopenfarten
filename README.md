# PPF-42 DYNAMICS · Aeroacoustic Physical Modeling Synthesizer
*Codenamed "Das Poopenfarten Modell 42"*

[![C++20](https://img.shields.io/badge/C%2B%2B-20-4A4A4A?style=for-the-badge&logo=cplusplus)](https://isocpp.org/)
[![JUCE 8](https://img.shields.io/badge/JUCE-8.0.x-EE592B?style=for-the-badge)](https://juce.com/)
[![Windows VST3 & Standalone](https://img.shields.io/badge/Windows-VST3%20%7C%20CLAP%20%7C%20Standalone-0078D6?style=for-the-badge&logo=windows)](build/PPF42_Dynamics_artefacts/Release/)
[![Verification: 100% PASS](https://img.shields.io/badge/Verification-100%25%20PASS%20(389%2F389)-24FF6A?style=for-the-badge&logo=checkmarx)](TEST_READY.md)
[![License: MIT](https://img.shields.io/badge/License-MIT-black?style=for-the-badge)](LICENSE)

> **A research-grade aeroacoustic relaxation synthesizer and gastrointestinal acoustic flow instrument.**  
> Built with continuous non-linear differential equations modeling lip-reed valve biomechanics, Reynolds turbulence noise, Minnaert fluid droplet resonance, and porcelain cavity acoustics.  
> Stealth precision-engineering branding: looks like a mastering processor at first glance; reveals hyper-realistic anatomical acoustics upon touch.

---

## 🎛️ At a Glance

* **100% Real-Time Mathematical Synthesis:** Zero static sample playback or wavetable shortcuts. Generates every waveform from first principles.
* **Modern Studio Pro Vector UI:** High-framerate 60 FPS hardware-accelerated dark-slate interface (`#0d1117`) rendered via Microsoft Edge WebView2, featuring an Aeroacoustic Particle Chamber and live Phosphor Oscilloscope.
* **Performance Control:** Dual master XY performance macros (*Gut Squeeze* and *Dietary Moisture*), dynamic pressure envelopes, portamento glide, and MPE pressure tracking.
* **Factory Library:** 10 curated anatomically authentic presets covering dry purrs, high-tension squeakers, wet multiphase splatters, and 18 Hz sub-bass rumbles.
* **FL Studio & DAW Compatibility:** Ships as 64-bit **VST3**, **CLAP**, and **Standalone Executable** (`.exe`).

---

## 🔬 Acoustic Physics & Signal Flow

```
[ MIDI Note / Pitch ] ──► [ Slew Limiter / Portamento ] ──► [ Sphincter Elasticity (f₀) ]
                                                                       │
[ MIDI Velocity / MPE ] ──► [ Gut Pressure Envelope ] ─────────────────┼────────┐
                                       │                               │        │
                                       ▼                               ▼        ▼
┌──────────────────────────────────────────────────────────────────────────────────┐
│ CORE AEROACOUSTIC ENGINE                                                         │
│                                                                                  │
│   ┌──────────────────────────┐          ┌────────────────────────────────────┐   │
│   │ Colonic Pressure Source  │ ── Bernoulli ──► Sphincter Relaxation Valve   │   │
│   │ (Dynamic Air Velocity)   │ ◄── Feedback ── (Mass-Spring-Damper Model)    │   │
│   └─────────────┬────────────┘          └─────────────────┬──────────────────┘   │
│                 │                                         │                      │
│                 ├───────────────────────┐                 │                      │
│                 ▼                       ▼                 ▼                      │
│   ┌──────────────────────────┐          ┌────────────────────────────────────┐   │
│   │ Aeroacoustic Noise Gen   │          │ Multiphase Fluid / Moisture Engine │   │
│   │ (Reynolds Jet Turbulence)│          │ (Minnaert Bubble & Droplet Pops)   │   │
│   └─────────────┬────────────┘          └─────────────────┬──────────────────┘   │
│                 │                                         │                      │
│                 └───────────────────────┬─────────────────┘                      │
│                                         ▼                                        │
│                            [ Summed Aeroacoustic Flow ]                          │
└─────────────────────────────────────────┼────────────────────────────────────────┘
                                          │
                                          ▼
┌──────────────────────────────────────────────────────────────────────────────────┐
│ CAVITY ACOUSTICS & MASTERING STAGE                                               │
│                                                                                  │
│   ┌──────────────────────────┐          ┌────────────────────────────────────┐   │
│   │ Intergluteal Cleft Filter│          │ Porcelain Cavity Convolver         │   │
│   │ (Boundary Waveguide Comb)│ ───────► │ (Zero-Latency Ceramic Bowl IRs)    │   │
│   └──────────────────────────┘          └─────────────────┬──────────────────┘   │
│                                                           │                      │
│   ┌──────────────────────────┐                            ▼                      │
│   │ Sub-Bass Reinforcement   │ ────────► ┌───────────────────────────────────┐   │
│   │ (Phase-Locked Sub Sine)  │           │ Saturator, Stereo Spatializer,    │   │
│   └──────────────────────────┘           │ & True-Peak Brickwall Limiter     │   │
│                                          └────────────────┬──────────────────┘   │
└───────────────────────────────────────────────────────────┼──────────────────────┘
                                                            │
                                                            ▼
                                                [ Stereo Output (L/R) ]
```

### 1. Sphincter Relaxation Oscillator
Models elastic tissue as an asymmetric non-linear mass-spring-damper valve under Bernoulli pressure differentials:
$$m \frac{d^2 y}{dt^2} + r \frac{dy}{dt} + k(y - y_0) = \Delta P(t)$$
* **Tension ($k$):** Directly tracks fundamental pitch ($18\text{ Hz} - 350\text{ Hz}$).
* **Aperture ($y_0$):** Dynamic resting clearance. Tight clearances induce sharp relaxation spikes; wider clearances allow high-velocity turbulent hissing.
* **Asymmetry / Flutter:** Injects Duffing non-linear damping for authentic subharmonic bifurcation and flutter-tongue stutter.

### 2. Reynolds Aeroacoustic Jet Turbulence
Calculates time-varying Reynolds number across the dynamic orifice to modulate turbulent noise:
$$N_{\text{turb}}(t) = \eta(t) \cdot |v_{\text{air}}(t)|^{2.5}$$

### 3. Multiphase Moisture Engine
* **Minnaert Bubble Radiator:** Resonates discrete acoustic micro-pulses modeling gas escaping through viscous liquid films:
  $$f_{\text{bubble}} = \frac{1}{2\pi R} \sqrt{\frac{3\gamma P_0}{\rho}}$$
* **Poisson Droplet Splatter:** Stochastic impulsive bursts generating wet pops and viscous squelches.

### 4. Porcelain Cavity Convolver
Zero-latency partitioned convolution engine loaded with modeled acoustic impulse responses:
* *Dry Direct Mic*
* *Ceramic Bowl - Standard* (Resonant standing waves at 400 Hz & 1.2 kHz)
* *Ceramic Bowl - Water Coupled* (Acoustic low-pass acoustic loading)
* *Tiled Enclosure* (Hard boundary specular reflections)

---

## 🎚️ Parameter Map

| Parameter ID | Display Name | Range | Description |
| :--- | :--- | :--- | :--- |
| `param_pressure` | **Colonic Drive** | 0.0 – 1.0 | Upstream pneumatic pressure driving the valve |
| `param_tension` | **Tissue Elasticity** | 18 – 350 Hz | Spring stiffness determining fundamental pitch |
| `param_aperture` | **Resting Aperture** | 0.0 – 1.0 | Equilibrium opening width of the orifice |
| `param_flutter` | **Asymmetry / Chaos** | 0.0 – 1.0 | Subharmonic bifurcation and flutter modulation |
| `param_viscosity` | **Fluid Viscosity** | 0.0 – 1.0 | Fluid mass damping and acoustic drag |
| `param_moisture` | **Dietary Moisture** | 0.0 – 1.0 | Minnaert bubble probability and droplet density |
| `param_droplet_rate`| **Splatter Rate** | 0.0 – 100/s | Poisson rate parameter for liquid droplet pops |
| `param_porcelain_mix`| **Porcelain Mix** | 0.0 – 1.0 | Dry/Wet blend of the ceramic chamber IR convolver |
| `param_porcelain_mode`| **Cavity Model** | 1 – 4 | Direct, Standard Bowl, Water Coupled, Tiled Room |
| `param_sub_level` | **Sub Reinforce** | -inf – +6 dB | Phase-locked clean sub-bass sine reinforcement |
| `param_voice_mode` | **Voice Mode** | 1 – 3 | Mono-Legato, Polyphonic (8-voice), Stereo Unison |
| `param_glide` | **Portamento** | 0 – 500 ms | Pitch glide slew rate |
| `macro_squeeze` | **Gut Squeeze** | 0.0 – 1.0 | Master performance macro: Drive + Tension + Chaos |
| `macro_moisture`| **Total Moisture** | 0.0 – 1.0 | Master performance macro: Viscosity + Droplet Density |

---

## 📁 Pre-Compiled Binaries

All Windows x64 binaries are built and ready for immediate deployment in `build/PPF42_Dynamics_artefacts/Release/`:

| Format | Path |
| :--- | :--- |
| **Standalone Application** | `build/PPF42_Dynamics_artefacts/Release/Standalone/PPF-42_Dynamics_Standalone.exe` |
| **VST3 Plugin (FL Studio)**| `build/PPF42_Dynamics_artefacts/Release/VST3/PPF-42_Dynamics.vst3` |
| **CLAP Plugin** | `build/PPF42_Dynamics_artefacts/Release/CLAP/PPF-42_Dynamics.clap` |
| **Headless C++ Test Runner**| `build/Release/ppf42_headless_dsp_tests.exe` |

### Installing in FL Studio:
1. Copy `PPF-42_Dynamics.vst3` into your standard VST3 folder:
   ```
   C:\Program Files\Common Files\VST3\
   ```
2. In FL Studio, open **Options** $\to$ **Manage plugins**.
3. Click **Find installed plugins**.
4. Search for **PPF-42 Dynamics** and add it to your channel rack.

---

## 🧪 Verification & Test Results

The engine was subjected to an independent multi-agent adversarial audit and test suite:

* **C++ Headless Test Suite (`ppf42_headless_dsp_tests.exe`):**
  * **37 / 37 Suites PASSED (100% PASS)** in 8.48 seconds.
  * Verified: 0 NaNs, 0 denormals, 0 audio thread heap allocations under `ScopedAllocationGuard`.
  * Verified: Peak limiter ceiling strictly $\le 0.0\text{ dBFS}$ under 100% Colonic Overdrive.
* **Automated Python E2E Test Suite (`tests/e2e/run_e2e_tests.py`):**
  * **352 / 352 Tests PASSED (100% PASS)** across all 5 tiers (Feature coverage, Boundary analysis, Pairwise interactions, DAW scenarios, and Adversarial stress).

---

## 🛠️ Compiling from Source

### Prerequisites:
* Windows 10/11 x64
* Visual Studio 2022 (with C++20 MSVC)
* CMake $\ge 3.22$

### Build:
```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Run test suite:
```bash
./build/Release/ppf42_headless_dsp_tests.exe
```

---

## 📜 License
MIT License. Published by Sneed's Feed & Seed Ltd.
