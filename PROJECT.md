# Project: PPF-42 DYNAMICS (Poopenfarten Pro)

## Architecture
- **Framework**: JUCE 8.0.x (C++20, CMake build system)
- **Target Binaries**: 
  - `PPF-42_Dynamics_Standalone.exe` (Windows x64 Standalone Executable, 4.62 MB)
  - `PPF-42_Dynamics.vst3` (VST3 Plugin Bundle, 3.40 MB)
  - `PPF-42_Dynamics.clap` (CLAP Plugin, 3.35 MB)
  - `ppf42_headless_dsp_tests.exe` (Automated Headless C++ DSP Verification Suite)
- **DSP Engine Architecture**: Decoupled static library (`ppf42_dsp_core`) with plain-old-data (POD) `ParameterSnapshot` and `MidiEvent` structures, operating without JUCE GUI or AudioProcessor dependencies. Sample-rate independent (44.1 kHz - 192 kHz), zero heap allocations during audio block processing.
- **Physical Modeling Mathematical Pipeline**:
  - Non-linear sphincter relaxation oscillator (mass-spring-damper Adachi/Fletcher valve + Bernoulli pressure coupling, semi-implicit Euler integration, 18 Hz - 350 Hz).
  - Aeroacoustic jet turbulence ($N_{\text{turb}}(t) = \eta(t) \cdot |v_{\text{air}}(t)|^{2.5}$).
  - Multiphase viscosity & moisture engine (Minnaert resonance bubble pulses + stochastic Poisson-distributed droplet pops).
  - Boundary cavity acoustics (intergluteal cleft waveguide comb filter + zero-latency porcelain impulse response convolver).
  - Low-end reinforcement & mastering (infrasonic 15 Hz DC blocker, sub-bass sine, $C^1$ cubic Hermite saturator, true-peak brickwall limiter $\le 0\text{ dBFS}$).
- **Voice Allocation**: Switchable Mono-Legato (with portamento glide slew and note priority stack), Polyphonic (8 voices with prioritized 4-tier voice stealing and 5 ms de-click crossfading), and Stereo Unison Detune.
- **UI Architecture**: JUCE 8 `juce::WebBrowserComponent` backed by Edge WebView2 on Windows. Modern dark-slate studio vector design (#0d1117 background, #161b22 panels, #30363d borders, neon cyan #58a6ff, amber #f0883e, acid emerald #39d353 accents).
- **Telemetry & IPC**: Coalesced 60 Hz bidirectional IPC parameter synchronization via atomic dirty flags and lock-free SPSC circular ring buffer for real-time Canvas visualizers (Aeroacoustic Particle Chamber, Relaxation Waveform Oscilloscope, 2D XY Performance Pad).

---

## Feature Inventory
| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | Non-Linear Relaxation Valve | Adachi/Fletcher mass-spring-damper valve model with Bernoulli pressure coupling | M1 | SPEC §3.1, R1.1 (VERIFIED) |
| 2 | Wide-Range Frequency Tracking | Calibrated fundamental frequency tracking across 18 Hz - 350 Hz | M1 | SPEC §3.1, R1.1 (VERIFIED) |
| 3 | Contact Stiffness & Restitution | Hard boundary collision at $y=0$ preventing negative aperture collapse | M1 | SPEC §3.1, spec_miner (VERIFIED) |
| 4 | Asymmetric Valve Flutter | Chaos and asymmetric damping for multiphonics and fluttering textures | M1 | SPEC §4.A, R1.1 (VERIFIED) |
| 5 | Dynamic Bernoulli Air Velocity | Instantaneous aperture airflow velocity computation from pressure head | M1 | SPEC §3.1, R1.1 (VERIFIED) |
| 6 | Reynolds Jet Turbulence Noise | Modulated aerodynamic hiss scaled to $|v_{\text{air}}|^{2.5}$ with bandpass coloring | M1 | SPEC §3.2, R1.2 (VERIFIED) |
| 7 | Minnaert Micro-Bubble Resonance | Stochastic pulsed damped sinusoidal resonators modeling gas-liquid bubbles | M1 | SPEC §3.3, R1.3 (VERIFIED) |
| 8 | Poisson Droplet Splatter | Stochastic transient droplet pops generated via inverse-CDF Poisson clock | M1 | SPEC §3.3, R1.3 (VERIFIED) |
| 9 | Multiphase Fluid Viscosity | Variable acoustic resistance and liquid mass loading | M1 | SPEC §3.3, R1.3 (VERIFIED) |
| 10 | Intergluteal Cleft Waveguide | Fractional delay comb filter simulating soft tissue acoustic reflection | M1 | SPEC §3.4, R1.4 (VERIFIED) |
| 11 | Porcelain Cavity Convolver | Zero-latency partitioned FIR convolver with 4 ceramic cavity modes | M1 | SPEC §3.4, R1.4 (VERIFIED) |
| 12 | Infrasonic 15 Hz DC Blocker | 1-pole highpass filter removing asymmetric DC offset while preserving 18 Hz | M1 | SPEC §3.5, explorer_dsp (VERIFIED) |
| 13 | Dedicated Sub-Bass Sine Layer | Phase-aligned low-end reinforcement oscillator | M1 | SPEC §3.5, R1.5 (VERIFIED) |
| 14 | Harmonic Saturation Drive | $C^1$ cubic Hermite soft-knee saturator for warm analogue warmth | M1 | SPEC §3.5, R1.5 (VERIFIED) |
| 15 | True-Peak Brickwall Limiter | Peak limiter preventing clipping ($y \le 0\text{ dBFS}$) under max drive | M1 | SPEC §3.5, R1.5 (VERIFIED) |
| 16 | Mono-Legato with Portamento | Note priority stack with configurable glide slew rate (0 - 500 ms) | M1 | SPEC §4.D, R1.6 (VERIFIED) |
| 17 | Polyphonic 8-Voice Mode | 8 physical modeling voices with 4-tier prioritized voice stealing | M1 | SPEC §4.D, R1.6 (VERIFIED) |
| 18 | Stereo Unison Detune Mode | Dual detuned stereo voice pairs with spatial dispersion | M1 | SPEC §4.D, R1.6 (VERIFIED) |
| 19 | MIDI & MPE Pressure Support | Continuous pressure modulation of Colonic Drive and pitch bend | M1 | SPEC §4.D, R1.6 (VERIFIED) |
| 20 | Sample-Rate Independence | Multi-rate coefficients and Euler timestep scaling across 44.1 - 192 kHz | M1 | SPEC §2, R2 (VERIFIED) |
| 21 | Zero Audio Allocation | Strict zero-allocation execution loop during audio processing | M1 | SPEC §2, R5 (VERIFIED) |
| 22 | Scoped Denormal Elimination | RAII hardware FTZ/DAZ mode and bitwise IEEE 754 finite verification | M1 | SPEC §2, R5 (VERIFIED) |
| 23 | APVTS Parameter Layout | Complete 22-parameter tree with thread-safe atomic access and ranges | M2 | SPEC §4, R2 (VERIFIED) |
| 24 | Performance Macros | Coupled non-linear transfer curves for `macro_squeeze` & `macro_moisture` | M2 | SPEC §4.E, R3.3 (VERIFIED) |
| 25 | 10 Anatomical Factory Presets | Curated physiological presets bundled in APVTS format | M2 | SPEC §6, R4 (VERIFIED) |
| 26 | Lock-Free SPSC Telemetry Ring | High-speed circular ring buffer streaming audio waveform & particle states | M2 | SPEC §2, R3.2 (VERIFIED) |
| 27 | Edge WebView2 UI Integration | JUCE 8 `juce::WebBrowserComponent` with Win32 DAW isolation flags | M2 | SPEC §2, R3 (VERIFIED) |
| 28 | Modern Dark-Slate Vector UI | Vital/FabFilter vector aesthetic (#0d1117 slate/obsidian + neon accents) | M2 | SPEC §5, R3.1 (VERIFIED) |
| 29 | Real-Time 60 FPS Visualizers | Canvas-based Aeroacoustic Particle Chamber and Spectral Oscilloscope | M2 | SPEC §5, R3.2 (VERIFIED) |
| 30 | Headless DSP Test Suite | Automated test executable validating stability, bounds, allocations, threads | M1 | SPEC §2, R5 (VERIFIED) |

---

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| M1 | Core DSP Engine & Headless Verification Suite | CMake build setup, `ppf42_dsp_core` physical modeling static library, and `ppf42_headless_dsp_tests.exe` (100k samples burn-in, 0 NaNs/denormals, peak limiter <= 0 dBFS, zero allocations) | none | DONE |
| M2 | JUCE Plugin Architecture, APVTS, Presets & Vector WebView UI | PluginProcessor wrapper, APVTS 22 parameters + macros, 10 presets, lock-free SPSC telemetry buffer, PluginEditor with Edge WebView2, Win32 clipping flags, modern dark-slate vector UI in `web/` with 60 FPS Canvas visualizers | M1 | DONE |
| M3 | Final Integration, E2E Test Suite Pass (Tiers 1-4) & Coverage Hardening (Tier 5) | Full desktop targets (Standalone .exe, VST3, CLAP), pass 100% of E2E tests from E2E Testing Track, Tier 5 adversarial hardening, and pre-victory verification | M1, M2, E2E Test Suite | DONE |

---

## Dual Track: E2E Testing Track
- **Status**: COMPLETE & VERIFIED
- **Deliverables**: `TEST_INFRA.md`, `TEST_READY.md`, `tests/e2e/*` (345/345 test cases passing 100%)
- **Tiers**:
  * Tier 1: 150/150 passed
  * Tier 2: 150/150 passed
  * Tier 3: 30/30 passed
  * Tier 4: 15/15 passed
- **Tier 5 Hardening**: 6 C++ adversarial suites added, 37/37 C++ suites passed.

---

## Verification Artifacts
- `build/Release/PPF-42_Dynamics_Standalone.exe` (4.62 MB) — Verified runnable
- `build/PPF-42_Dynamics_artefacts/Release/VST3/PPF-42_Dynamics.vst3` (3.40 MB) — Verified valid VST3 bundle
- `build/PPF-42_Dynamics_artefacts/Release/CLAP/PPF-42_Dynamics.clap` (3.35 MB) — Verified valid CLAP plugin
- `build/Release/ppf42_headless_dsp_tests.exe` (175 KB) — 37/37 C++ test suites passed
- `tests/e2e/run_e2e_tests.py` — 345/345 tests passed (100.0%)
