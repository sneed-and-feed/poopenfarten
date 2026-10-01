# E2E Test Infra: PPF-42 DYNAMICS (Poopenfarten Pro)
# Aeroacoustic Physical Modeling Synthesizer

**Document ID**: `PPF42-TEST-INFRA-001`  
**Target Subsystems**: Native C++20 DSP Core (`ppf42_dsp_core`), JUCE 8 VST3 / CLAP / Standalone Plugin Targets, Edge WebView2 GUI (`web/`), and Headless Verification Executable (`ppf42_headless_dsp_tests.exe`)  
**Standard Compliance**: ISO/IEC C++20, IEEE 754-2019 Floating-Point Arithmetic, MIDI 1.0 & MPE Specification, RFC 8259 JSON, Webview2 Runtime Specs  
**Status**: Authoritative Test Infrastructure Specification & 4-Tier Automated Test Framework  

---

## 1. Test Philosophy

The testing methodology for **PPF-42 DYNAMICS** (*Poopenfarten Pro*) is founded upon five uncompromising architectural principles:

1. **Opaque-Box, Requirement-Driven Verification**:
   All test cases are derived strictly from user requirements in `ORIGINAL_REQUEST.md`, architectural specifications in `PROJECT.md`, and authoritative mathematical physics documented in `PPF42_DYNAMICS_SPEC.md` and `spec_inventory.md`. Tests evaluate observable acoustic output waveforms, audio buffer properties, state invariants, parameter dynamics, mathematical convergence, and protocol compliance without coupling to private internal classes or implementation details.

2. **Zero Tolerance for Facades or Circumvention**:
   No trivial "always-pass" tests, mock assertions, or tautological checks are permitted. Every test asserts against real computational DSP algorithms, true floating-point audio blocks, physical differential equation solutions, spectral metrics, or strict hardware-level execution invariants.

3. **Explicit Authoritative Expected Output Derivation**:
   For every single test case across all 4 tiers, the expected output is derived from an authoritative source:
   - *Mathematical Oracles*: Closed-form and numerical solutions to the Adachi/Fletcher valve reed differential equation ($m \ddot{y} + r \dot{y} + k(y - y_0) = \Delta P$), Bernoulli pressure flow equation ($v_{\text{air}} = \text{sgn}(\Delta P)\sqrt{2|\Delta P|/\rho}$), Minnaert acoustic bubble resonance ($f_b = \frac{1}{2\pi R}\sqrt{3\gamma P_0 / \rho}$), Poisson process inter-arrival statistics ($P(k) = \frac{\lambda^k e^{-\lambda}}{k!}$), and cubic Hermite soft-saturation curves.
   - *Physical Conservation Laws*: Non-negative aperture constraint ($y \ge 0$), energy conservation during valve collision restitution, mass loading damping increases, and acoustic pressure volume velocity $U(t) = w \cdot \max(0, y) \cdot v_{\text{air}}$.
   - *Audio Engineering Standards*: Master brickwall limiter ceiling strictly $\le 0.0\text{ dBFS}$ ($1.000$ amplitude), infrasonic DC blocker attenuation below $15\text{ Hz}$, and frequency tracking bounded to $[18, 350]\text{ Hz}$.
   - *Format & System Specifications*: APVTS parameter ranges and skew factors, RFC 8259 JSON IPC schemas, 60 FPS telemetry ring buffer contracts, and Windows `WS_CLIPCHILDREN | WS_CLIPSIBLINGS` host hygiene styles.

4. **Multi-Tier Orthogonal Partitioning**:
   The verification space is structured into four progressive, non-overlapping tiers:
   - **Tier 1 (Category-Partition Feature Coverage)**: Validates baseline functionality, primary happy paths, and nominal parameter spaces across all 30 features F01 through F30 ($\ge 5$ test cases per feature = $\ge 150$ tests).
   - **Tier 2 (Boundary Value Analysis & Corner Cases)**: Exercises mathematical extrema, boundary collisions, zero-state inputs, multi-rate scaling ($44.1\text{ kHz}$ to $192\text{ kHz}$), denormal flushing, and hardware limits across all 30 features ($\ge 5$ test cases per feature = $\ge 150$ tests).
   - **Tier 3 (Pairwise Combinatorial Interactions)**: Systematically tests cross-coupling between independent subsystems, verifying that concurrent modulation, DSP stages, and macro controls interact stably without phase nulls, numerical divergence, or race conditions ($\ge 30$ interaction tests).
   - **Tier 4 (Real-World Application Workloads)**: Simulates complete end-to-end studio production workflows, FL Studio automation clips, polyphonic keyboard performances, extreme preset morphs, and continuous burn-in stress sessions ($15$ realistic production workloads).
   *Total Test Suite Volume: $\ge 345$ automated test cases.*

5. **Self-Containment & Progressive Testability**:
   Every test case is completely self-contained, idempotent, and isolated. Tests instantiate their own data structures, configure explicit parameter states, process deterministically, verify assertions, and release resources with zero global side effects or execution order dependencies.

---

## 2. Feature Inventory Coverage Matrix (30 Features)

The complete feature inventory defined in `PROJECT.md § Feature Inventory` is mapped across the 4 verification tiers:

| # | Feature ID | Feature Name | Subsystem Category | Milestone | Tier 1 (Min 5) | Tier 2 (Min 5) | Tier 3 (Pairwise) | Tier 4 (Workloads) |
|---|:----------:|--------------|:------------------:|:---------:|:--------------:|:--------------:|:-----------------:|:------------------:|
| 1 | **F01** | Non-Linear Relaxation Valve | Exciter Mechanics | M1 | 5 | 5 | [X] | S01, S04, S10 |
| 2 | **F02** | Wide-Range Frequency Tracking | Exciter Mechanics | M1 | 5 | 5 | [X] | S01, S02, S10 |
| 3 | **F03** | Contact Stiffness & Restitution | Exciter Mechanics | M1 | 5 | 5 | [X] | S02, S05, S14 |
| 4 | **F04** | Asymmetric Valve Flutter | Exciter Mechanics | M1 | 5 | 5 | [X] | S05, S08, S09 |
| 5 | **F05** | Dynamic Bernoulli Air Velocity | Aerodynamics | M1 | 5 | 5 | [X] | S01, S07, S11 |
| 6 | **F06** | Reynolds Jet Turbulence Noise | Aerodynamics | M1 | 5 | 5 | [X] | S02, S07, S11 |
| 7 | **F07** | Minnaert Micro-Bubble Resonance | Fluid Dynamics | M1 | 5 | 5 | [X] | S03, S06, S13 |
| 8 | **F08** | Poisson Droplet Splatter | Fluid Dynamics | M1 | 5 | 5 | [X] | S03, S06, S13 |
| 9 | **F09** | Multiphase Fluid Viscosity | Fluid Dynamics | M1 | 5 | 5 | [X] | S03, S06, S13 |
| 10 | **F10** | Intergluteal Cleft Waveguide | Cavity Acoustics | M1 | 5 | 5 | [X] | S01, S04, S12 |
| 11 | **F11** | Porcelain Cavity Convolver | Cavity Acoustics | M1 | 5 | 5 | [X] | S04, S06, S12 |
| 12 | **F12** | Infrasonic 15 Hz DC Blocker | Low-End / Mastering | M1 | 5 | 5 | [X] | S04, S10, S15 |
| 13 | **F13** | Dedicated Sub-Bass Sine Layer | Low-End / Mastering | M1 | 5 | 5 | [X] | S04, S10, S15 |
| 14 | **F14** | Harmonic Saturation Drive | Low-End / Mastering | M1 | 5 | 5 | [X] | S05, S10, S14 |
| 15 | **F15** | True-Peak Brickwall Limiter | Low-End / Mastering | M1 | 5 | 5 | [X] | S04, S05, S14 |
| 16 | **F16** | Mono-Legato with Portamento | Voice Management | M1 | 5 | 5 | [X] | S01, S08, S11 |
| 17 | **F17** | Polyphonic 8-Voice Mode | Voice Management | M1 | 5 | 5 | [X] | S07, S11, S15 |
| 18 | **F18** | Stereo Unison Detune Mode | Voice Management | M1 | 5 | 5 | [X] | S09, S11, S15 |
| 19 | **F19** | MIDI & MPE Pressure Support | MIDI & Control | M1 | 5 | 5 | [X] | S01, S08, S11 |
| 20 | **F20** | Sample-Rate Independence | Numerics / Architecture | M1 | 5 | 5 | [X] | S04, S14, S15 |
| 21 | **F21** | Zero Audio Allocation | Real-Time Safety | M1 | 5 | 5 | [X] | S14, S15 |
| 22 | **F22** | Scoped Denormal Elimination | Real-Time Numerics | M1 | 5 | 5 | [X] | S14, S15 |
| 23 | **F23** | APVTS Parameter Layout | Host Architecture | M2 | 5 | 5 | [X] | S01-S15 |
| 24 | **F24** | Performance Macros | Modulation / Macros | M2 | 5 | 5 | [X] | S01, S03, S13 |
| 25 | **F25** | 10 Anatomical Factory Presets | Preset System | M2 | 5 | 5 | [X] | S01-S10 |
| 26 | **F26** | Lock-Free SPSC Telemetry Ring | Visualizer Telemetry | M2 | 5 | 5 | [X] | S12, S13, S14 |
| 27 | **F27** | Edge WebView2 UI Integration | UI Architecture | M2 | 5 | 5 | [X] | S12, S13 |
| 28 | **F28** | Modern Dark-Slate Vector UI | UI Presentation | M2 | 5 | 5 | [X] | S12, S13 |
| 29 | **F29** | Real-Time 60 FPS Visualizers | UI Visualizers | M2 | 5 | 5 | [X] | S12, S13 |
| 30 | **F30** | Headless DSP Test Suite | Automated Verification | M1 | 5 | 5 | [X] | All Workloads |

---

## 3. Systematic 4-Tier Test Architecture

```
===================================================================================================
                       PPF-42 DYNAMICS AUTOMATED 4-TIER E2E TEST SUITE
===================================================================================================

  +---------------------------------------------------------------------------------------------+
  | TIER 1: FEATURE COVERAGE (Category-Partition Methodology)                                   |
  | - Minimum 5 tests per feature for all 30 features (F01 - F30) = 150 Tests                   |
  | - Validates primary operational pathways, nominal ranges, and functional state transitions   |
  | - Explicit mathematical verification against differential equations and physical laws        |
  +---------------------------------------------------------------------------------------------+

  +---------------------------------------------------------------------------------------------+
  | TIER 2: BOUNDARY VALUE ANALYSIS & CORNER CASES                                              |
  | - Minimum 5 tests per feature for all 30 features (F01 - F30) = 150 Tests                   |
  | - Boundary conditions: 18 Hz floor, 350 Hz ceiling, zero pressure, maximum overdrive        |
  | - Multi-rate verification (44.1 kHz, 48 kHz, 88.2 kHz, 96 kHz, 176.4 kHz, 192 kHz)         |
  | - Zero audio allocation invariant, IEEE 754 bitwise finite checks, denormal flushing        |
  +---------------------------------------------------------------------------------------------+

  +---------------------------------------------------------------------------------------------+
  | TIER 3: CROSS-FEATURE PAIRWISE COMBINATIONS                                                 |
  | - 30 orthogonal cross-feature interaction test cases                                        |
  | - Gut Squeeze + Dietary Moisture macro cross-coupling                                       |
  | - MPE Pressure modulation + Porcelain Cavity Convolver modal excitation                     |
  | - Polyphonic voice stealing + Infrasonic DC Blocker transient stability                     |
  | - Portamento glide + Stereo Unison Detune spatial phase coherence                          |
  +---------------------------------------------------------------------------------------------+

  +---------------------------------------------------------------------------------------------+
  | TIER 4: REAL-WORLD APPLICATION SCENARIOS                                                    |
  | - 15 realistic sound design & FL Studio DAW automation workloads                            |
  | - Presets 01-10 auditioning, club sub-rumble 808 bursts, rapid parameter automations        |
  | - Continuous 100,000-sample burn-in stress test under full random parameter modulation      |
  +---------------------------------------------------------------------------------------------+

Total Automated Test Suite Volume: 345 Test Cases (100% Pass Threshold Required)
===================================================================================================
```

### 3.1 Tier 1: Category-Partition Feature Coverage (150 Test Cases)

Tier 1 partitions each of the 30 features into distinct operational categories, establishing baseline functional correctness:

- **F01 (Non-Linear Relaxation Valve)**:
  1. `T1_F01_01_ValveEquilibrium`: Resting aperture displacement matches equilibrium $y_0$ under zero driving pressure.
  2. `T1_F01_02_PressureOscillation`: Trans-sphincteric pressure differential initiates sustained relaxation oscillation cycles.
  3. `T1_F01_03_DynamicClosure`: Increased air velocity induces Bernoulli pressure drop pulling aperture displacement toward zero.
  4. `T1_F01_04_VolumeVelocity`: Acoustic volume velocity $U(t) = w \cdot \max(0, y) \cdot v_{\text{air}}$ generates positive acoustic pulses.
  5. `T1_F01_05_TensionStiffness`: Increasing tissue elasticity $k$ increases natural oscillation frequency.

- **F02 (Wide-Range Frequency Tracking)**:
  1. `T1_F02_01_PitchTrackingMid`: Standard MIDI Note 48 (130.81 Hz) tracks within $\pm 0.5\%$ frequency accuracy.
  2. `T1_F02_02_SubBassFloor`: Low MIDI notes cleanly track down to 18.0 Hz fundamental.
  3. `T1_F02_03_HighSqueakCeiling`: Upper MIDI notes cleanly track up to 350.0 Hz fundamental.
  4. `T1_F02_04_PitchBendContinuous`: MIDI pitch bend wheel smoothly scales fundamental frequency proportionally.
  5. `T1_F02_05_TensionMicroAdjust`: Fine tissue tension parameter shifts fundamental without discontinuities.

- **F03 (Contact Stiffness & Restitution)**:
  1. `T1_F03_01_ApertureNonNegative`: Tissue displacement never penetrates past boundary ($y \ge 0$).
  2. `T1_F03_02_ContactRestitution`: Impact velocity reverses with coefficient $e_{\text{restitution}}$ upon boundary contact.
  3. `T1_F03_03_ImpulsiveShockWave`: Hard boundary collision produces characteristic high-frequency harmonic click.
  4. `T1_F03_04_ContactDamping`: Collision damping prevents unphysical energy accumulation at $y = 0$.
  5. `T1_F03_05_SpringNonLinearity`: Non-linear contact stiffness escalates smoothly near closure plane.

- **F04 (Asymmetric Valve Flutter)**:
  1. `T1_F04_01_FlutterSubharmonics`: Engaging flutter parameter ($> 0.3$) generates audible period-doubling subharmonics.
  2. `T1_F04_02_AsymmetricDamping`: Directional damping asymmetry distorts sinusoidal velocity profile.
  3. `T1_F04_03_FlappingTexture`: High flutter settings ($0.8 - 1.0$) produce chaotic flapping envelope modulation.
  4. `T1_F04_04_DuffingStiffness`: Cubic stiffness non-linearity creates amplitude-dependent pitch drift.
  5. `T1_F04_05_ZeroFlutterClean`: Flutter $= 0.0$ yields pure periodic relaxation oscillation without subharmonic hash.

- **F05 (Dynamic Bernoulli Air Velocity)**:
  1. `T1_F05_01_VelocityMonotonicity`: Higher upstream colonic pressure produces higher peak air velocity.
  2. `T1_F05_02_BernoulliSuction`: Peak airflow velocity correlates with peak negative suction pressure.
  3. `T1_F05_03_ZeroPressureZeroVelocity`: Airflow velocity drops strictly to $0.0$ when $\Delta P \le 0.0$.
  4. `T1_F05_04_GasDensityScaling`: Air velocity scales inversely with the square root of gas density $\rho_{\text{gas}}$.
  5. `T1_F05_05_FlowDecoupling`: When valve is closed ($y = 0$), transmitted flow volume drops to zero.

- **F06 (Reynolds Jet Turbulence Noise)**:
  1. `T1_F06_01_TurbulenceAirVelocityScaling`: Turbulent noise power scales with $|v_{\text{air}}|^{2.5}$.
  2. `T1_F06_02_ApertureModulation`: Jet hiss amplitude is modulated by instantaneous aperture area.
  3. `T1_F06_03_PinkFilterSpectrum`: Kellet pinking filter produces $-3\text{ dB/octave}$ spectral tilt.
  4. `T1_F06_04_ZeroNoiseWhenClosed`: Turbulence noise is completely suppressed when valve is sealed ($y \le 0$).
  5. `T1_F06_05_NoiseAperiodicPurity`: PRNG stochastic source exhibits zero repetitive periodicity or tone artifacts.

- **F07 (Minnaert Micro-Bubble Resonance)**:
  1. `T1_F07_01_MinnaertFrequencyBounds`: Bubble burst frequencies fall strictly within $800\text{ Hz} - 6.5\text{ kHz}$.
  2. `T1_F07_02_RadiusInversion`: Smaller bubble radii produce proportionally higher resonant frequencies.
  3. `T1_F07_03_DampedSineWavepacket`: Individual bubble pulses decay exponentially according to viscosity damping ratio.
  4. `T1_F07_04_MoistureDensityCoupling`: Higher moisture parameters increase bubble trigger probability per audio block.
  5. `T1_F07_05_ZeroMoistureZeroBubbles`: Moisture $= 0.0$ completely silences bubble generator.

- **F08 (Poisson Droplet Splatter)**:
  1. `T1_F08_01_PoissonExponentialIntervals`: Inter-arrival intervals follow exponential distribution matching Poisson process.
  2. `T1_F08_02_DropletRateScaling`: Parameter `param_droplet_rate` linearly scales mean event frequency $\lambda$.
  3. `T1_F08_03_ResonantPopFilter`: Droplet pops trigger resonant bandpass filters centered between $1.2\text{ kHz}$ and $4.8\text{ kHz}$.
  4. `T1_F08_04_TransientImpulseSharpness`: Droplet pops exhibit sharp transient attacks (< 1 ms rise time).
  5. `T1_F08_05_ZeroSplatterWhenDry`: Setting `param_droplet_rate = 0.0` or `param_moisture = 0.0` yields zero droplet spikes.

- **F09 (Multiphase Fluid Viscosity)**:
  1. `T1_F09_01_ViscosityMassLoading`: Increasing viscosity parameter increases effective tissue mass $m_{\text{eff}}$.
  2. `T1_F09_02_FundamentalPitchDrop`: Viscosity mass loading lowers fundamental oscillation pitch.
  3. `T1_F09_03_AcousticDampingIncrease`: Higher viscosity increases high-frequency damping in the acoustic path.
  4. `T1_F09_04_BubbleDecayLengthening`: High viscosity lengthens bubble decay envelope.
  5. `T1_F09_05_TimbreDarkening`: Spectral centroid shifts downward as viscosity increases from $0.0$ to $1.0$.

- **F10 (Intergluteal Cleft Waveguide)**:
  1. `T1_F10_01_CombDelayGeometry`: Delay length corresponds to physical cleft dimensions ($\approx 1.2\text{ ms}$).
  2. `T1_F10_02_HarmonicFiltering`: Comb filter produces characteristic periodic boundary notches in the frequency domain.
  3. `T1_F10_03_TissueDampingAbsorption`: Parameter `param_cleft_damping` rolls off high-frequency reflections in comb loop.
  4. `T1_F10_04_FractionalDelayInterpolation`: Catmull-Rom cubic Hermite interpolation guarantees click-free delay scaling.
  5. `T1_F10_05_FeedbackStability`: Feedback gain strictly maintained below $0.95$ for unconditional stability.

- **F11 (Porcelain Cavity Convolver)**:
  1. `T1_F11_01_ZeroLatencyPartition`: First partition executed in time-domain with exactly zero sample algorithmic delay.
  2. `T1_F11_02_Model0DryChamber`: Model 0 produces transparent close-mic reflection without bowl coloration.
  3. `T1_F11_03_Model1CeramicBowl`: Model 1 exhibits resonant modal peaks around $420\text{ Hz}$ and $1180\text{ Hz}$.
  4. `T1_F11_04_Model2WaterCoupled`: Model 2 attenuates frequencies above $600\text{ Hz}$ simulating liquid absorption.
  5. `T1_F11_05_Model3TiledEnclosure`: Model 3 renders dense flutter echoes with longer decay time ($RT_{60} \approx 1.2\text{ s}$).

- **F12 (Infrasonic 15 Hz DC Blocker)**:
  1. `T1_F12_01_DCElimination`: Complete elimination of static DC bias ($0\text{ Hz}$ transmission $= 0.000$).
  2. `T1_F12_02_Preserve18HzSubBass`: Flat magnitude response at $18.0\text{ Hz}$ with attenuation $< 1.5\text{ dB}$.
  3. `T1_F12_03_PoleStability`: Pole radius $R = 1.0 - \frac{2\pi \cdot 15}{f_s}$ clamped strictly $< 0.99995$ preventing ringing.
  4. `T1_F12_04_TransientRecovery`: DC blocker settles back to baseline within $250\text{ ms}$ following large step offset.
  5. `T1_F12_05_ZeroPhaseDistortionInAudibleBand`: Linear phase behavior maintained above $40\text{ Hz}$.

- **F13 (Dedicated Sub-Bass Sine Layer)**:
  1. `T1_F13_01_SinePurity`: Sub-bass generator outputs pure sinusoidal tone with harmonic distortion $< -60\text{ dBc}$.
  2. `T1_F13_02_FundamentalPhaseLock`: Sub-bass phase is continuously locked to the relaxation oscillator's $f_0$.
  3. `T1_F13_03_SubLevelDbRange`: Sub level parameter calibrates smoothly from $-\infty\text{ dB}$ (mute) to $+6\text{ dB}$.
  4. `T1_F13_04_SubGainSmoothing`: Parameter changes to sub level are de-clicked via one-pole smoothing filter.
  5. `T1_F13_05_PitchTrackingSync`: Sub-bass pitch tracks note transitions instantaneously.

- **F14 (Harmonic Saturation Drive)**:
  1. `T1_F14_01_LinearRegionPreservation`: Inputs below knee threshold ($|x| < 0.72$) pass with exact unity gain.
  2. `T1_F14_02_SoftKneeHermiteTransition`: $C^1$ cubic Hermite spline ensures smooth derivative at knee boundary.
  3. `T1_F14_03_HarmonicGeneration`: Saturation drive enriches audio with odd and subtle even harmonic warmth.
  4. `T1_F14_04_DriveParameterScaling`: Drive parameter smoothly increases saturation density from subtle to aggressive.
  5. `T1_F14_05_SaturatorCeilingClamp`: Saturated output never exceeds maximum boundary ceiling $M = 1.05$.

- **F15 (True-Peak Brickwall Limiter)**:
  1. `T1_F15_01_Absolute0dBFSClamping`: Master output signal is strictly bounded to $\le 1.0000$ ($0.0\text{ dBFS}$).
  2. `T1_F15_02_OverdriveCeilingIntegrity`: Driving master input $+24\text{ dB}$ into limiting maintains peak ceiling $\le 1.0000$.
  3. `T1_F15_03_TransparentLowLevel`: Signals below limiting knee pass completely uncompressed.
  4. `T1_F15_04_MasterGainStage`: `param_master_gain` accurately boosts or attenuates signal prior to brickwall stage.
  5. `T1_F15_05_LimiterDenormalFlush`: Limiter intercepts and flushes any NaN or denormal input to zero.

- **F16 (Mono-Legato with Portamento)**:
  1. `T1_F16_01_SingleVoiceEnforcement`: Only 1 voice active at any time in Mono-Legato mode.
  2. `T1_F16_02_LegatoEnvelopeRetriggerSuppression`: Overlapping Note On maintains continuous pressure sustain without retrigger.
  3. `T1_F16_03_PortamentoSlew`: Pitch glides smoothly between notes over `param_glide_time` duration.
  4. `T1_F16_04_InstantZeroGlide`: When glide time $= 0\text{ ms}$, pitch changes instantaneously on note change.
  5. `T1_F16_05_NoteStackReturn`: Releasing overlapping note glides pitch back to previously held note in priority stack.

- **F17 (Polyphonic 8-Voice Mode)**:
  1. `T1_F17_01_EightVoiceSimultaneous`: Up to 8 concurrent notes sound simultaneously with independent pitch and envelopes.
  2. `T1_F17_02_OldestVoiceStealing`: Playing 9th note steals oldest active voice without stalling the engine.
  3. `T1_F17_03_DeClickFadeRamp`: Stolen voice executes 5 ms de-click release fade eliminating waveform pops.
  4. `T1_F17_04_PolyVelocityMapping`: MIDI velocity dynamically scales initial colonic pressure head per voice.
  5. `T1_F17_05_IndependentVoiceFilter`: Each polyphonic voice maintains private state variables and filter states.

- **F18 (Stereo Unison Detune Mode)**:
  1. `T1_F18_01_MultiVoiceStacking`: Single MIDI note triggers 2 or 4 unison voice cores.
  2. `T1_F18_02_StereoPanningDispersion`: Voices are panned symmetrically across left and right channels.
  3. `T1_F18_03_DetuneCentSpread`: Voice pairs are detuned by calibrated spreads ($\pm 7\text{ cents}$, $\pm 14\text{ cents}$).
  4. `T1_F18_04_PhaseDecorrelation`: Unison voices initialize with staggered phase offsets ($0^\circ, 90^\circ, 180^\circ, 270^\circ$).
  5. `T1_F18_05_EnergyNormalization`: Total stereo output power is normalized by $1/\sqrt{N_{\text{voices}}}$.

- **F19 (MIDI & MPE Pressure Support)**:
  1. `T1_F19_01_ChannelAftertouchMapping`: Standard MIDI Channel Pressure modulates Colonic Drive in real time.
  2. `T1_F19_02_MPEPolyPressure`: MPE polyphonic pressure modulates pressure head per individual note.
  3. `T1_F19_03_PitchBendRange`: Standard $\pm 2$ semitone pitch bend range tracks pitch wheel smoothly.
  4. `T1_F19_04_ModWheelVibrato`: Modulation wheel (CC#1) introduces subtle flutter and pressure vibrato.
  5. `T1_F19_05_AllNotesOffSupport`: MIDI All-Notes-Off (CC#123) and All-Sound-Off (CC#120) release all active voices immediately.

- **F20 (Sample-Rate Independence)**:
  1. `T1_F20_01_SampleRate44k`: Full DSP engine operates stably at 44.1 kHz.
  2. `T1_F20_02_SampleRate48k`: Full DSP engine operates stably at 48.0 kHz.
  3. `T1_F20_03_SampleRate96k`: Full DSP engine operates stably at 96.0 kHz with proportional timestep scaling.
  4. `T1_F20_04_SampleRate192k`: Full DSP engine operates stably at 192.0 kHz without numerical divergence.
  5. `T1_F20_05_ConsistentCentroids`: Spectral centroid of identical preset matches within $\pm 2\%$ across all sample rates.

- **F21 (Zero Audio Allocation)**:
  1. `T1_F21_01_SteadyStateZeroAlloc`: Audio render callback performs 0 calls to `malloc` / `new`.
  2. `T1_F21_02_ParamSweepZeroAlloc`: Rapid parameter sweeps during audio processing perform 0 heap allocations.
  3. `T1_F21_03_MidiBurstZeroAlloc`: Dense MIDI note flurries during audio processing perform 0 heap allocations.
  4. `T1_F21_04_VoiceStealingZeroAlloc`: Rapid voice stealing cycles perform 0 heap allocations.
  5. `T1_F21_05_TelemetryPushZeroAlloc`: Pushing visualizer frames to telemetry queue performs 0 heap allocations.

- **F22 (Scoped Denormal Elimination)**:
  1. `T1_F22_01_HardwareFTZDAZ`: `ScopedNoDenormals` successfully sets MXCSR FTZ/DAZ bits on x64.
  2. `T1_F22_02_BitwiseFiniteCheck`: `isFiniteBitwise` correctly identifies NaNs and infinities.
  3. `T1_F22_03_SubnormalFlush`: Numbers smaller than $10^{-15}$ are flushed to bit-exact $0.0\text{f}$.
  4. `T1_F22_04_DecaySilenceStallPrevention`: Filter feedback decaying into silence never causes CPU microcode stalls.
  5. `T1_F22_05_RAIIRestoration`: `ScopedNoDenormals` restores original CPU control register upon exiting scope.

- **F23 (APVTS Parameter Layout)**:
  1. `T1_F23_01_All22ParamsRegistered`: All 22 required parameters exist with unique string IDs in APVTS.
  2. `T1_F23_02_DefaultValuesAccurate`: Default parameter values match specification table exactly.
  3. `T1_F23_03_RangeMinMaxBounds`: Parameter values clamp strictly between documented minimum and maximum limits.
  4. `T1_F23_04_SkewFactorConformity`: Logarithmic parameters (glide, envelope times) apply correct skew curves.
  5. `T1_F23_05_AtomicRawAccess`: Audio thread accesses parameters via lock-free atomic pointer reads.

- **F24 (Performance Macros)**:
  1. `T1_F24_01_SqueezeModulatesDrive`: `macro_squeeze` scales effective colonic pressure head.
  2. `T1_F24_02_SqueezeModulatesTension`: `macro_squeeze` scales effective tissue elasticity spring constant.
  3. `T1_F24_03_SqueezeModulatesFlutter`: `macro_squeeze` quadratically scales flutter chaos.
  4. `T1_F24_04_MoistureModulatesViscosity`: `macro_moisture` scales fluid viscosity acoustic loading.
  5. `T1_F24_05_MoistureModulatesDroplets`: `macro_moisture` superlinearly scales droplet splatter density.

- **F25 (10 Anatomical Factory Presets)**:
  1. `T1_F25_01_Preset01CleanPurr`: Preset 01 loads valid parameters and produces warm, dry relaxation oscillation.
  2. `T1_F25_02_Preset02Squeaker`: Preset 02 loads high tension ($0.92$) and narrow aperture ($0.08$) whistling tone.
  3. `T1_F25_03_Preset03Splatter`: Preset 03 loads high moisture ($0.80$) and viscosity ($0.85$) with squelchy droplets.
  4. `T1_F25_04_Preset04SubRumble`: Preset 04 loads relaxed tension ($0.12$) producing 18 Hz sub-bass displacement.
  5. `T1_F25_05_Preset05To10Conformity`: Presets 05 through 10 load valid parameter configurations matching catalog.

- **F26 (Lock-Free SPSC Telemetry Ring)**:
  1. `T1_F26_01_QueuePushPop`: Pushing a visualizer frame from writer thread allows immediate pop on reader thread.
  2. `T1_F26_02_FifoOrdering`: Frames are popped in strict First-In, First-Out sequence.
  3. `T1_F26_03_NonBlockingWhenFull`: Pushing to a full telemetry queue drops oldest frame without blocking caller.
  4. `T1_F26_04_EmptyCheckReturnsFalse`: Popping from an empty telemetry queue returns false immediately.
  5. `T1_F26_05_ScopeSampleDecimation`: Telemetry frame captures 128 decimated waveform samples accurately.

- **F27 (Edge WebView2 UI Integration)**:
  1. `T1_F27_01_WindowStyleFlags`: Plugin HWND applies `WS_CLIPCHILDREN | WS_CLIPSIBLINGS` styles.
  2. `T1_F27_02_HostIsolationArguments`: Launch flags include `--mute-audio` and `--disable-web-midi`.
  3. `T1_F27_03_ResourceMimeTypes`: WebResourceManager serves `.html`, `.css`, and `.js` with correct MIME headers.
  4. `T1_F27_04_EmbeddedZipPackaging`: Binary web assets package extracts without disk path dependencies.
  5. `T1_F27_05_BidirectionalIpcBridge`: Native IPC interface routes parameter updates to WebView and receives gestures.

- **F28 (Modern Dark-Slate Vector UI)**:
  1. `T1_F28_01_PaletteObsidianBackground`: Theme root background declares deep obsidian slate `#0d1117`.
  2. `T1_F28_02_PanelBorders`: Card surfaces use `#161b22` with hairline borders `#30363d`.
  3. `T1_F28_03_NeonCyanAccents`: Mechanical & Pitch controls utilize Neon Cyan `#58a6ff`.
  4. `T1_F28_04_AmberGoldAccents`: Pressure & Drive controls utilize Amber Gold `#f0883e`.
  5. `T1_F28_05_AcidEmeraldAccents`: Moisture & Droplet controls utilize Acid Emerald `#39d353`.

- **F29 (Real-Time 60 FPS Visualizers)**:
  1. `T1_F29_01_ParticleChamber200`: Aeroacoustic particle chamber instantiates 200 dynamic flow particles.
  2. `T1_F29_02_ParticleVelocityCoupling`: Particle horizontal speed scales with instantaneous airflow velocity.
  3. `T1_F29_03_ApertureCenterGraphic`: Particle canvas renders physical sphincter aperture opening/closing.
  4. `T1_F29_04_PhosphorOscilloscopeDecay`: Scope canvas implements multi-frame exponential phosphor persistence.
  5. `T1_F29_05_IdleThrottlingTo2Hz`: Telemetry and rendering throttle down to 2 Hz during extended audio silence.

- **F30 (Headless DSP Test Suite)**:
  1. `T1_F30_01_ExecutableBuilds`: Headless test executable compiles as an independent console binary.
  2. `T1_F30_02_OfflineExecution`: Tests run 100% offline without audio device or display server.
  3. `T1_F30_03_ExitCodeZeroOnPass`: Process returns exit code 0 when all tests pass.
  4. `T1_F30_04_AssertionDiagnostics`: Failed assertions output file, line number, and diagnostic values.
  5. `T1_F30_05_MicrosecondProfiling`: Test runner tracks and reports elapsed execution time per test suite.

---

### 3.2 Tier 2: Boundary Value Analysis & Corner Cases (150 Test Cases)

Tier 2 pushes all 30 features to computational, physical, and numerical extremes:

- **F01 (Valve Mechanics Extremes)**:
  1. `T2_F01_01_ZeroColonicPressure`: $\Delta P = 0.0$ keeps valve at resting gap $y_0$ with zero audio output.
  2. `T2_F01_02_MaxColonicPressure`: $\Delta P = 1.0$ under minimum damping; assert stable numerical convergence.
  3. `T2_F01_03_NegativeDisplacementShock`: Initializing $y < 0$ recovers to $y \ge 0$ in $\le 1$ sample step.
  4. `T2_F01_04_ZeroApertureRestingGap`: $y_0 = 0.0$ forces valve into tight relaxation regime without divide-by-zero.
  5. `T2_F01_05_ExtremeTissueMass`: Evaluating minimum and maximum tissue mass boundaries.

- **F02 (Frequency Tracking Boundaries)**:
  1. `T2_F02_01_Exact18HzBoundary`: Note frequency clamped strictly at 18.0 Hz floor for MIDI note 0.
  2. `T2_F02_02_Exact350HzBoundary`: Note frequency clamped strictly at 350.0 Hz ceiling for MIDI note 127.
  3. `T2_F02_03_PitchBendMaxDown`: Pitch bend wheel fully down (-8192) clamps cleanly at 18.0 Hz.
  4. `T2_F02_04_PitchBendMaxUp`: Pitch bend wheel fully up (+8191) clamps cleanly at 350.0 Hz.
  5. `T2_F02_05_RapidChromaticHammer`: 100 chromatic note transitions in 50 ms execute without phase explosions.

- **F03 (Restitution & Collision Boundaries)**:
  1. `T2_F03_01_RestitutionZero`: $e_{\text{restitution}} = 0.0$ (inelastic collision) stops downward velocity immediately.
  2. `T2_F03_02_RestitutionUnity`: $e_{\text{restitution}} = 1.0$ (perfect elastic rebound) conserves kinetic energy.
  3. `T2_F03_03_HighVelocityImpact`: Impact velocity $v = -100\text{ m/s}$ does not cause penetration $y < 0$.
  4. `T2_F03_04_ChatterCollisionLimit`: High-frequency boundary chattering resolves without infinite loops.
  5. `T2_F03_05_SubSampleContact`: Contact force interpolation prevents overshoot at coarse sample rates.

- **F04 (Flutter Chaos Boundaries)**:
  1. `T2_F04_01_FlutterMaxDriveMax`: Flutter $= 1.0$ with Colonic Drive $= 1.0$; assert bounded displacement $|y| \le 2.0$.
  2. `T2_F04_02_FlutterMinDriveMin`: Flutter $= 0.0$ with Colonic Drive $= 0.0$; valve decays to exact rest.
  3. `T2_F04_03_AsymmetricDampingSignFlip`: Sign flipping during zero-crossing does not create DC offset drift.
  4. `T2_F04_04_CubicStiffnessClamp`: Large aperture displacement clamps non-linear stiffness preventing divergence.
  5. `T2_F04_05_BifurcationStability`: Phase portrait remains inside bounded limit cycle attractor.

- **F05 (Air Velocity Boundaries)**:
  1. `T2_F05_01_NegativePressureHead`: Negative upstream pressure clamps air velocity strictly to $0.0$.
  2. `T2_F05_02_InstantaneousPressureSpike`: Pressure step from $0.0$ to $1.0$ in 1 sample does not create NaN velocity.
  3. `T2_F05_03_AirDensityNearZero`: Near-zero gas density clamped internally to prevent division by zero.
  4. `T2_F05_04_SupersonicVelocityLimit`: Air velocity bounded by acoustic Mach number ceiling.
  5. `T2_F05_05_VelocityZeroWhenGapZero`: Orifice velocity times zero aperture area yields exact zero flow.

- **F06 (Turbulence Noise Boundaries)**:
  1. `T2_F06_01_ZeroVelocityZeroTurbulence`: When $v_{\text{air}} = 0$, noise output is bit-exact $0.0\text{f}$.
  2. `T2_F06_02_MaxVelocityNoiseBounded`: Maximum velocity does not cause pink filter overflow ($|N_{\text{turb}}| \le 1.0$).
  3. `T2_F06_03_PRNGPeriodExhaustion`: 1,000,000 PRNG samples maintain zero DC bias ($\text{mean} < 0.001$).
  4. `T2_F06_04_FilterPoleStabilityAt192k`: Pink filter poles remain strictly inside unit circle at 192 kHz.
  5. `T2_F06_05_FilterPoleStabilityAt44k`: Pink filter poles remain stable at 44.1 kHz.

- **F07 (Minnaert Bubble Boundaries)**:
  1. `T2_F07_01_MinBubbleRadius`: Radius $R = 0.5\text{ mm}$ produces maximum resonant frequency $6.5\text{ kHz}$.
  2. `T2_F07_02_MaxBubbleRadius`: Radius $R = 4.0\text{ mm}$ produces minimum resonant frequency $800\text{ Hz}$.
  3. `T2_F07_03_MaxConcurrentVoices`: Polyphonic bubble manager caps simultaneous active bubbles at 16.
  4. `T2_F07_04_VoiceStealingWhenFull`: 17th bubble burst steals quietest active bubble voice cleanly.
  5. `T2_F07_05_MoistureMaxRateMax`: Moisture $= 1.0$ and Droplet Rate $= 1.0$ never saturate audio bus.

- **F08 (Poisson Droplet Boundaries)**:
  1. `T2_F08_01_EventProbabilityClamp`: Event inception probability per sample clamped to $\le 0.05$.
  2. `T2_F08_02_InverseCdfLogZero`: Uniform random draw $u \to 1.0$ handled safely without $\ln(0)$ infinity.
  3. `T2_F08_03_PopFilterBiquadNyquist`: Upper pop frequency ($4.8\text{ kHz}$) stable at 44.1 kHz sample rate.
  4. `T2_F08_04_ZeroArrivalRate`: $\lambda = 0.0$ prevents all droplet triggers.
  5. `T2_F08_05_BurstClustering`: Multiple droplets triggered in adjacent samples do not cause filter clipping.

- **F09 (Viscosity Loading Boundaries)**:
  1. `T2_F09_01_ViscosityZero`: Viscosity $= 0.0$ imposes no added mass ($m_{\text{eff}} = m$) or damping.
  2. `T2_F09_02_ViscosityUnity`: Viscosity $= 1.0$ applies maximum mass ($m_{\text{eff}} = 1.85m$) stably.
  3. `T2_F09_03_ViscosityTransitionSmooth`: Sweeping viscosity $0 \to 1$ during note hold produces smooth pitch drop.
  4. `T2_F09_04_CleftWaveguideDampingMax`: Viscosity combined with max cleft damping prevents high-frequency oscillation.
  5. `T2_F09_05_BubbleDampingRatioMax`: Maximum viscosity sets bubble damping ratio $\zeta = 0.15$.

- **F10 (Waveguide Comb Boundaries)**:
  1. `T2_F10_01_MinDelayLength`: Delay length at 192 kHz scales accurately to fractional sample delay.
  2. `T2_F10_02_MaxDampingFilterRollOff`: Max boundary damping eliminates reflections above $2\text{ kHz}$.
  3. `T2_F10_03_FeedbackGainLimit`: Waveguide feedback gain clamped to $0.95$ preventing runaway resonance.
  4. `T2_F10_04_CombBufferWraparound`: Delay buffer circular pointer wrapping verified at buffer boundaries.
  5. `T2_F10_05_ZeroDampingStability`: Minimum damping ($0.0$) maintains stable comb ring without clipping.

- **F11 (Convolver Boundaries)**:
  1. `T2_F11_01_MixZeroBypass`: Porcelain Mix $= 0.0$ completely bypasses convolution calculation.
  2. `T2_F11_02_MixUnity100PercentWet`: Porcelain Mix $= 1.0$ routes $100\%$ wet convolved reverberation.
  3. `T2_F11_03_SizeRescaleMin0_5`: Porcelain Size $= 0.5$ shifts modal resonances up $2\times$ without artifacts.
  4. `T2_F11_04_SizeRescaleMax2_0`: Porcelain Size $= 2.0$ shifts modal resonances down $0.5\times$ smoothly.
  5. `T2_F11_05_ModelSwitchDuringAudio`: Switching between models 0, 1, 2, 3 during audio rendering causes zero pops.

- **F12 (DC Blocker Boundaries)**:
  1. `T2_F12_01_MassiveDCOffsetStep`: DC step of $+10.0$ is completely blocked within $250\text{ ms}$.
  2. `T2_F12_02_18HzPassBandRipple`: Attenuation at 18 Hz is strictly $< 1.5\text{ dB}$ across all sample rates.
  3. `T2_F12_03_HighFrequencyPass`: $1\text{ kHz}$ and $10\text{ kHz}$ signals pass through DC blocker with $0.00\text{ dB}$ loss.
  4. `T2_F12_04_SampleRateScalingPole`: Pole coefficient $R$ computed properly for 44.1 kHz, 96 kHz, and 192 kHz.
  5. `T2_F12_05_DenormalFlushingInFilter`: Filter state variables flushed to zero when signal decays below $10^{-15}$.

- **F13 (Sub-Bass Boundaries)**:
  1. `T2_F13_01_SubLevelMinus60Db`: Sub level at $-60\text{ dB}$ produces inaudible output ($< 0.001$).
  2. `T2_F13_02_SubLevelPlus6Db`: Sub level at $+6\text{ dB}$ boosts sine amplitude by factor of $2.0$.
  3. `T2_F13_03_SubPhaseContinuousPitchBend`: Continuous pitch modulation maintains phase continuity (zero clicks).
  4. `T2_F13_04_Sub18HzSinePurity`: Sub-bass at 18.0 Hz produces pure single spectral peak.
  5. `T2_F13_05_SubBassMonoCenter`: Sub-bass sine is centered identically in Left and Right channels.

- **F14 (Saturation Drive Boundaries)**:
  1. `T2_F14_01_DriveZeroUnity`: Drive $= 0.0$ preserves clean signal dynamics below knee $0.72$.
  2. `T2_F14_02_DriveMaxPlus18Db`: Drive $= 1.0$ applies $4\times$ gain into Hermite soft knee safely.
  3. `T2_F14_03_ExtremeInputBursts`: Input of $+40\text{ dBFS}$ ($100.0$ amplitude) bounded strictly to $1.05$.
  4. `T2_F14_04_DerivativeSmoothnessAtKnee`: First derivative of transfer function continuous at $|x| = 0.72$.
  5. `T2_F14_05_SymmetricClipping`: Positive and negative peaks saturate symmetrically without introducing DC.

- **F15 (Limiter Boundaries)**:
  1. `T2_F15_01_CeilingUnder24DbOverdrive`: Input $+24\text{ dB}$ above full scale clamped strictly $\le 1.0000$.
  2. `T2_F15_02_ZeroOvershoot`: Maximum peak across 50,000 overdriven samples strictly $\le 1.000000$.
  3. `T2_F15_03_NegativePeakClamping`: Negative peaks clamped strictly $\ge -1.000000$.
  4. `T2_F15_04_MasterGainMinMinus60`: Master gain at $-60\text{ dB}$ attenuates signal to silence.
  5. `T2_F15_05_MasterGainMaxPlus12`: Master gain at $+12\text{ dB}$ drives limiter cleanly without numeric breakdown.

- **F16 (Portamento Boundaries)**:
  1. `T2_F16_01_GlideTimeZero`: Glide time $= 0\text{ ms}$ steps pitch in exactly 1 sample.
  2. `T2_F16_02_GlideTimeMax500ms`: Glide time $= 500\text{ ms}$ slews pitch smoothly over full half-second.
  3. `T2_F16_03_ReverseGlideDirection`: Gliding upward then abruptly triggering lower note reverses trajectory smoothly.
  4. `T2_F16_04_LogarithmicPitchSlew`: Frequency slewing follows exponential curve matching musical pitch perception.
  5. `T2_F16_05_RapidLegatoTrill`: 20 Hz trill between two held notes slews stably without phase wrap.

- **F17 (Polyphonic Voice Stealing Boundaries)**:
  1. `T2_F17_01_Concurrent16NotesHammer`: Striking 16 notes at once steals 8 voices smoothly with de-click fades.
  2. `T2_F17_02_ZeroSampleDeClickPops`: Stolen voice waveform crossfade confirms zero discontinuity clicks.
  3. `T2_F17_03_QuietVoiceStealing`: Voice stealing prioritizes quietest/released voices before held voices.
  4. `T2_F17_04_AllNotesOffVoiceClearing`: MIDI All Notes Off silences all 8 voices into release stage.
  5. `T2_F17_05_ActiveVoiceIndexPacking`: Inactive voices bypassed during block rendering to preserve CPU.

- **F18 (Unison Detune Boundaries)**:
  1. `T2_F18_01_ExtremeDetuneStability`: Unison detuning at extreme spreads remains phase-stable.
  2. `T2_F18_02_StereoBalanceSum`: Left and right channel total power remains balanced within $\pm 0.1\text{ dB}$.
  3. `T2_F18_03_MonoCompatibilityCheck`: Summing unison stereo output to mono does not produce comb cancellation nulls.
  4. `T2_F18_04_TwoVoiceMode`: Stacking 2 voices spreads $\pm 7\text{ cents}$ symmetrically.
  5. `T2_F18_05_FourVoiceMode`: Stacking 4 voices spreads $\pm 7$ and $\pm 14\text{ cents}$ symmetrically.

- **F19 (MPE & Control Boundaries)**:
  1. `T2_F19_01_MPEPressureZero`: Pressure $= 0$ drops voice drive to minimum resting state.
  2. `T2_F19_02_MPEPressureMax127`: Pressure $= 127$ boosts drive to maximum calibrated headroom.
  3. `T2_F19_03_RapidPressureModulation`: 1000 pressure updates/sec processed smoothly with one-pole smoothing.
  4. `T2_F19_04_PitchBendBoundaryLimits`: Pitch bend values $-8192$ and $+8191$ handle full numeric range safely.
  5. `T2_F19_05_VelocityZeroNoteOn`: MIDI Note On with velocity 0 treated as Note Off per MIDI standard.

- **F20 (Multi-Rate Multi-Block Boundaries)**:
  1. `T2_F20_01_SingleSampleBlockSize`: Processing block size of 1 sample executes correctly.
  2. `T2_F20_02_MaxBlockSize4096`: Processing block size of 4096 samples executes correctly.
  3. `T2_F20_03_DynamicBlockSizeSwitching`: Alternating block sizes (64, 512, 1, 1024) across blocks causes zero glitches.
  4. `T2_F20_04_SampleRateSwitching`: Re-preparing engine from 44.1 kHz to 192 kHz resets coefficients cleanly.
  5. `T2_F20_05_NyquistLimitClamping`: All internal filter cutoffs clamped below $0.49 f_s$.

- **F21 (Real-Time Heap Allocation Audit)**:
  1. `T2_F21_01_ProcessBlockZeroHeapAlloc`: Overloaded `operator new` confirms 0 allocations in 500 audio blocks.
  2. `T2_F21_02_MaxDriveMaxVoicesZeroAlloc`: 8 voices playing with max saturation performs 0 allocations.
  3. `T2_F21_03_ConvolverProcessingZeroAlloc`: Convolver audio rendering executes with 0 dynamic allocations.
  4. `T2_F21_04_VoiceStealingZeroAlloc`: Voice stealing and de-clicking executes with 0 dynamic allocations.
  5. `T2_F21_05_ParameterModulationZeroAlloc`: Rapid parameter automation executes with 0 dynamic allocations.

- **F22 (Denormal & NaN Rejection Boundaries)**:
  1. `T2_F22_01_InjectNaNInput`: Injecting NaN into audio inputs flushes to $0.0\text{f}$ without propagation.
  2. `T2_F22_02_InjectInfInput`: Injecting $+\infty$ or $-\infty$ flushes to $0.0\text{f}$ immediately.
  3. `T2_F22_03_InjectDenormalInput`: Injecting $1.0 \times 10^{-35}$ flushes to bit-exact $0.0\text{f}$.
  4. `T2_F22_04_DecaySubnormalLoop`: Reverb tail decaying over 10 seconds flushes to zero without CPU spike.
  5. `T2_F22_05_BitwiseIEEEValidation`: Every output sample in 100,000 sample test passes `isFiniteBitwise`.

- **F23 (Parameter Range Clamping Boundaries)**:
  1. `T2_F23_01_BelowMinClamp`: Host passing $-1.0$ to `param_pressure` clamps to minimum $0.0$.
  2. `T2_F23_02_AboveMaxClamp`: Host passing $+2.0$ to `param_pressure` clamps to maximum $1.0$.
  3. `T2_F23_03_ChoiceParamOutBounds`: Host passing choice index 99 to `param_voice_mode` clamps to 2.
  4. `T2_F23_04_LogarithmicSkewLimits`: Slew times approach zero and max without division by zero.
  5. `T2_F23_05_ConcurrentParameterWrites`: Atomic writes from GUI thread read safely on audio thread.

- **F24 (Macro Boundary Scaling)**:
  1. `T2_F24_01_MacroSqueezeZero`: `macro_squeeze = 0.0` maps smoothly to relaxed base parameters.
  2. `T2_F24_02_MacroSqueezeUnity`: `macro_squeeze = 1.0` maps smoothly to max colonic drive and tension.
  3. `T2_F24_03_MacroMoistureZero`: `macro_moisture = 0.0` completely bypasses wetness engines.
  4. `T2_F24_04_MacroMoistureUnity`: `macro_moisture = 1.0` triggers maximum viscosity and droplet rates.
  5. `T2_F24_05_MacroCornerClamp`: Base parameter at max combined with macro at max clamps safely $\le 1.0$.

- **F25 (Preset State Boundary Integrity)**:
  1. `T2_F25_01_PresetIndexOutOfBounds`: Selecting preset index -1 or 10 clamps safely to valid range.
  2. `T2_F25_02_PresetLoadingCrossFade`: Switching presets during sustained audio crossfades without pops.
  3. `T2_F25_03_PresetStateXmlRoundTrip`: Saving and restoring APVTS state XML preserves exact float values.
  4. `T2_F25_04_PresetAllParametersPresent`: Every preset defines all 22 parameters explicitly.
  5. `T2_F25_05_PresetAudioBounds`: All 10 presets produce peak output $\le 0.0\text{ dBFS}$ when played.

- **F26 (Telemetry Queue Stress Boundaries)**:
  1. `T2_F26_01_TelemetryUnderflowSafety`: Reader thread popping 10,000 times from empty queue never crashes.
  2. `T2_F26_02_TelemetryOverflowSafety`: Audio thread pushing 10,000 frames into full queue drops frames cleanly.
  3. `T2_F26_03_TelemetryPowerOfTwoMask`: Queue capacity is exact power of two ($32$) with bitwise index wrapping.
  4. `T2_F26_04_SPSCThreadContention`: Simultaneous high-rate push/pop on separate threads causes zero data corruption.
  5. `T2_F26_05_TelemetryNanGuard`: Telemetry frames never contain NaNs or infinite values.

- **F27 (WebView2 Platform Flag Boundaries)**:
  1. `T2_F27_01_EnvVarStringLength`: Environment variable string passes valid Win32 bounds.
  2. `T2_F27_02_HwndStyleBitmaskIntegrity`: `WS_CLIPCHILDREN | WS_CLIPSIBLINGS` bits correctly set on target HWND.
  3. `T2_F27_03_MimeHeaderHandling`: Unknown file extension defaults safely to `application/octet-stream`.
  4. `T2_F27_04_IPCMessageBufferMax`: JSON messages up to 64 KB parsed cleanly without buffer overrun.
  5. `T2_F27_05_BrowserClosedSafety`: DSP runs uninterrupted when UI window is closed or destroyed.

- **F28 (Vector UI Rendering Boundaries)**:
  1. `T2_F28_01_HexColorParsers`: All hex color constants match CSS `#0d1117`, `#58a6ff`, `#f0883e`, `#39d353`.
  2. `T2_F28_02_MinCanvasDimensions`: Canvas handles resize to minimum dimensions ($100 \times 100$) without crashing.
  3. `T2_F28_03_HighDpiPixelRatio`: UI scales correctly at DPI scale factors $1.0\times, 1.25\times, 1.5\times, 2.0\times$.
  4. `T2_F28_04_TouchCoordinateClamping`: XY pad touch coordinates outside bounds clamp strictly to $[0, 1]$.
  5. `T2_F28_05_TabularNumsMonospace`: Monospace numeric styling prevents layout shifting during parameter sweeps.

- **F29 (Visualizer Frame Rate Boundaries)**:
  1. `T2_F29_01_ParticleCountThrottling`: Visualizer throttles particle count if frame time exceeds $20\text{ ms}$.
  2. `T2_F29_02_SilenceDetectionThreshold`: RMS below $0.001$ triggers 2 Hz low-power idle refresh mode.
  3. `T2_F29_03_ZeroCrossingFallback`: Oscilloscope falls back cleanly if no zero-crossing detected in buffer.
  4. `T2_F29_04_CanvasContextLoss`: Visualizer recovers cleanly if WebGL/Canvas context is lost and restored.
  5. `T2_F29_05_ParticleBoundsReflection`: Particles bouncing off chamber boundaries stay strictly inside canvas.

- **F30 (Headless Runner Extreme Stress)**:
  1. `T2_F30_01_100kSampleBurnIn`: 100,000 samples rendered under extreme pseudo-random parameter automation sweeps.
  2. `T2_F30_02_ZeroAssertionsFailed`: Zero assertion failures recorded across all 100,000 burn-in samples.
  3. `T2_F30_03_BurnInZeroNaNs`: Every sample in 100,000 burn-in verified finite via `isFiniteBitwise`.
  4. `T2_F30_04_BurnInPeakLimiting`: Maximum absolute sample during 100,000 burn-in strictly $\le 1.0000$.
  5. `T2_F30_05_MemoryLeakAbsence`: Memory footprint remains constant throughout burn-in execution.

---

### 3.3 Tier 3: Cross-Feature Pairwise Combinations (30 Test Cases)

Tier 3 exercises pairwise interactions between distinct subsystems to verify seamless multi-module integration:

1. `T3_01`: **Gut Squeeze Macro (F24) + Dietary Moisture Macro (F24)**: Simultaneous automation across 2D XY pad; verifies smooth dual-macro modulation without cross-talk artifacts.
2. `T3_02`: **MPE Pressure (F19) + Porcelain Cavity Convolver (F11)**: Dynamic pressure surges driving ceramic bowl standing wave resonances without feedback runaway.
3. `T3_03`: **Portamento Glide (F16) + Stereo Unison Detune (F18)**: Polyphonic pitch slewing across 4 detuned unison voices maintaining stereo width and phase alignment.
4. `T3_04`: **Asymmetric Flutter (F04) + Infrasonic 15 Hz DC Blocker (F12)**: Chaotic subharmonic flapping producing substantial DC offset; DC blocker eliminates bias while preserving low end.
5. `T3_05`: **Minnaert Micro-Bubbles (F07) + Harmonic Saturation Drive (F14)**: High-frequency bubble bursts passing through cubic Hermite saturator without high-frequency aliasing hash.
6. `T3_06`: **Poisson Droplet Splatter (F08) + Intergluteal Cleft Waveguide (F10)**: Transient droplet pops exciting cleft boundary comb filter creating metallic squelch echoes.
7. `T3_07`: **Reynolds Jet Noise (F06) + Porcelain Cavity Convolver (F11)**: Turbulent air hiss diffused through Tiled Enclosure impulse response creating authentic room air acoustics.
8. `T3_08`: **Polyphonic Voice Stealing (F17) + Infrasonic DC Blocker (F12)**: Rapid voice reallocation steps do not introduce low-frequency thumps or DC thumps into master bus.
9. `T3_09`: **Dedicated Sub-Bass Sine (F13) + True-Peak Brickwall Limiter (F15)**: 18 Hz sub-bass sine layer boosted to $+6\text{ dB}$ into limiter; verifies ceiling $\le 1.0000$ and low THD.
10. `T3_10`: **Multiphase Viscosity (F09) + Frequency Tracking (F02)**: Viscosity mass loading dropping fundamental pitch; frequency tracker maintains calibrated MIDI interval tracking.
11. `T3_11`: **Sphincter Relaxation Valve (F01) + Lock-Free Telemetry (F26)**: Valve displacement and velocity pushed in real-time to telemetry buffer without dropping audio blocks.
12. `T3_12`: **Contact Restitution (F03) + Master Saturation (F14)**: Hard valve boundary collisions producing sharp transient spikes softened cleanly by Hermite saturation stage.
13. `T3_13`: **Dynamic Bernoulli Air Velocity (F05) + Aeroacoustic Particle Chamber (F29)**: Real-time air velocity telemetry driving particle stream acceleration on UI canvas.
14. `T3_14`: **10 Anatomical Factory Presets (F25) + APVTS Parameter Layout (F23)**: Rapidly cycling all 10 presets updates all 22 APVTS parameter trees accurately and thread-safely.
15. `T3_15`: **Mono-Legato Portamento (F16) + Dedicated Sub-Bass Sine (F13)**: Portamento frequency slewing smoothly glides both relaxation valve and phase-locked sub-bass oscillator.
16. `T3_16`: **Stereo Unison Detune (F18) + Harmonic Saturation Drive (F14)**: Detuned unison beat frequencies driving saturator without producing unpleasant intermodulation distortion.
17. `T3_17`: **Polyphonic 8-Voice Mode (F17) + Zero Audio Allocation (F21)**: Full 8-voice polyphonic chord struck simultaneously executes with bit-exact 0 heap allocations.
18. `T3_18`: **Sample-Rate Independence (F20) + Intergluteal Cleft Waveguide (F10)**: Waveguide delay length scales with sample rate ($44.1\text{ kHz} \to 192\text{ kHz}$) maintaining identical comb comb response.
19. `T3_19`: **Minnaert Bubble Radiator (F07) + Poisson Droplet Engine (F08)**: High moisture setting concurrently triggering bubbles and droplets with bounded polyphony.
20. `T3_20`: **Edge WebView2 UI (F27) + Lock-Free Telemetry (F26)**: JavaScript UI bridge polling telemetry queue at 60 Hz while audio thread renders without lock contention.
21. `T3_21`: **True-Peak Limiter (F15) + Scoped Denormal Elimination (F22)**: Limiter and saturator processing near-zero signals flush denormals while maintaining ceiling.
22. `T3_22`: **MPE Pressure (F19) + Gut Squeeze Macro (F24)**: MPE pressure automating Colonic Drive while Gut Squeeze automates tension and flutter simultaneously.
23. `T3_23`: **Porcelain Convolver Size Rescaling (F11) + Ceramic Bowl Model (F11)**: Dynamically modulating `param_porcelain_size` shifts ceramic modal frequencies without clicks.
24. `T3_24`: **Harmonic Saturation Drive (F14) + True-Peak Limiter (F15)**: Extreme drive settings ($+18\text{ dB}$) saturating into brickwall limiter strictly bounded to $0\text{ dBFS}$.
25. `T3_25`: **Aperture Resting Gap (F01) + Reynolds Jet Turbulence (F06)**: Modulating resting aperture controls transition from pure turbulent air hiss to tight valve popping.
26. `T3_26`: **Modern Dark Slate UI (F28) + 2D XY Vector Pad (F24)**: Vector pad dragging smoothly sends coalesced IPC messages updating `macro_squeeze` and `macro_moisture`.
27. `T3_27`: **Factory Preset Migration (F25) + Phosphor Oscilloscope (F29)**: Switching presets instantaneously reflects new characteristic relaxation waveform on oscilloscope trace.
28. `T3_28`: **Infrasonic 15 Hz DC Blocker (F12) + 18 Hz Frequency Tracking (F02)**: Deep 18 Hz sub-rumble passes through DC blocker with $< 1.5\text{ dB}$ attenuation while DC is stripped.
29. `T3_29`: **Stereo Unison Detune (F18) + Porcelain Cavity Convolver (F11)**: Panned unison voices convolved through stereo ceramic impulse response maintaining wide spatial image.
30. `T3_30`: **Headless DSP Test Suite (F30) + 100k Sample Burn-In (F22)**: Automated headless test runner validates entire physical modeling pipeline across 100,000 samples.

---

### 3.4 Tier 4: Real-World Application Workloads (15 Scenarios)

Tier 4 simulates comprehensive end-to-end studio production sessions, DAW automation, sound design workflows, and endurance burn-in:

- **S01: Factory Preset 01 "Clean Continental Purr" Audition**:
  - Configuration: Preset 01 loaded, Mono-Legato mode, MIDI Note 36 (65.4 Hz), medium colonic drive ($0.65$), dry cavity ($0.20$ mix).
  - Assertion: Stable periodic relaxation oscillation, warm dry timbre, peak amplitude bounded, zero denormals.
- **S02: Factory Preset 02 "High-Tension Squeaker" Whistling Lead**:
  - Configuration: Preset 02 loaded, high tension ($0.92$), narrow aperture ($0.08$), MIDI Note 60 (261.6 Hz), portamento $25\text{ ms}$.
  - Assertion: High fundamental squeak ($> 250\text{ Hz}$), prominent jet air hiss, smooth pitch glide, bounded output.
- **S03: Factory Preset 03 "Viscous Multiphase Splatter" Wet Bassline**:
  - Configuration: Preset 03 loaded, high moisture ($0.80$), viscosity ($0.85$), droplet rate ($0.75$), Water Coupled bowl.
  - Assertion: Dense micro-bubble bursts, Poisson transient pops, dark low-pass timbre, zero audio dropouts.
- **S04: Factory Preset 04 "Visceral Sub-Rumble (18 Hz)" Club Sub Test**:
  - Configuration: Preset 04 loaded, MIDI Note 12, fundamental tracked to 18.0 Hz floor, sub-bass sine at $+3\text{ dB}$, Drive $0.45$.
  - Assertion: Extreme low-frequency displacement, DC blocker eliminates static bias, true-peak limiter clamps $\le 1.0000$.
- **S05: Factory Preset 05 "Flutter-Tongue Stutter" Chaotic Flapping**:
  - Configuration: Preset 05 loaded, flutter chaos at $0.95$, Drive $0.20$, Dry chamber model.
  - Assertion: Prominent period-doubling subharmonics, chaotic envelope modulation, bounded limit cycle stability.
- **S06: Factory Preset 06 "Wet Porcelain Slam" Resonant Bathroom Slap**:
  - Configuration: Preset 06 loaded, high moisture ($0.75$), Porcelain mix $0.75$, Ceramic Bowl standard model ($420\text{ Hz}$ resonance).
  - Assertion: Wet squelch attack convolved into distinct ceramic cavity resonant modes, zero algorithmic latency.
- **S07: Factory Preset 07 "Micro-Puff Staccato" Percussive Click Sequence**:
  - Configuration: Preset 07 loaded, rapid 16th-note staccato pulses (attack $0.5\text{ ms}$, decay $60\text{ ms}$, sustain $0.0$), Zero glide.
  - Assertion: Sharp percussive transient clicks, instantaneous envelope decay, zero click bleeding into release.
- **S08: Factory Preset 08 "Extended Gaseous Drift" Slow Drone**:
  - Configuration: Preset 08 loaded, long decay ($1800\text{ ms}$), sustain $0.75$, Tiled Enclosure model, gentle flutter vibrato.
  - Assertion: Continuous evolving air turbulence, dense reverberant tail, zero denormal slowdown over 5 seconds.
- **S09: Factory Preset 09 "Unison Twin Cannons" Massive Stereo Bass**:
  - Configuration: Preset 09 loaded, Stereo Unison mode (4 voices, $\pm 7$ and $\pm 14$ cents detune, stereo pan dispersion).
  - Assertion: Immense stereo width, rich choral beating, mono compatibility without total phase cancellation.
- **S10: Factory Preset 10 "The Brown Note 808" Trap Sub Hit**:
  - Configuration: Preset 10 loaded, maximum Colonic Drive ($0.95$), locked Sub-Bass sine at $+6\text{ dB}$, Drive $0.55$, Master Gain $+1.0\text{ dB}$.
  - Assertion: Massive punchy 808-style low-end transient decaying into clean sub-bass, brickwall limiter prevents hard clipping.
- **S11: FL Studio Piano Roll Polyphonic Chord Progression**:
  - Configuration: Polyphonic 8-voice mode, rapid 4-note jazz chord changes (C2 min9 $\to$ F2 13 $\to$ Bb1 maj7) with note overlap.
  - Assertion: Voice manager allocates independent voices cleanly, stolen voices fade out over 5 ms without clicks.
- **S12: Live DAW Automation Sweep across 2D XY Performance Pad**:
  - Configuration: Continuous circular automation of `macro_squeeze` and `macro_moisture` over 10 seconds.
  - Assertion: Real-time non-linear parameter mapping, continuous timbre morphing, zero zipper noise or audio glitches.
- **S13: Full Telemetry Streaming & 60 FPS Visualizer Frame Audit**:
  - Configuration: Active audio playback streaming visualizer frames to lock-free SPSC ring buffer; reader pops 600 frames.
  - Assertion: Particle chamber and oscilloscope frames populated accurately, zero buffer underruns, zero audio thread blocking.
- **S14: Multi-Rate Dynamic Block Size DAW Stress Workout**:
  - Configuration: Cycling sample rates (44.1k, 48k, 96k, 192k) with dynamic block sizes (1, 16, 64, 512, 2048) under full load.
  - Assertion: Proportional timestep scaling, zero buffer overflows, zero memory allocations, total numerical stability.
- **S15: 100,000-Sample Endurance Burn-In with Random Parameter Modulation**:
  - Configuration: Unbroken processing of 100,000 continuous samples with randomized parameter modulations every 64 samples.
  - Assertion: Exactly 0 NaNs, 0 infinities, 0 denormals, peak amplitude $\le 1.0000$ across all 100,000 samples.

---

## 4. Coverage Thresholds & Compliance Metrics

| Metric | Required Minimum | Specified in Suite | Compliance Margin |
|:-------|:----------------:|:------------------:|:-----------------:|
| **Tier 1 (Feature Coverage)** | $\ge 150$ ($5 \times 30$ features) | **150 tests** | Exact ($5.0$ tests / feature) |
| **Tier 2 (Boundary & Corner Cases)** | $\ge 150$ ($5 \times 30$ features) | **150 tests** | Exact ($5.0$ tests / feature) |
| **Tier 3 (Cross-Feature Interactions)** | $\ge 30$ pairwise tests | **30 tests** | Exact ($100\%$ target) |
| **Tier 4 (Real-World Application Scenarios)** | $\ge 15$ studio workloads | **15 workloads** | Exact ($100\%$ target) |
| **Total Test Suite Volume** | **$\ge 345$ tests** | **345 tests** | **Exceeds Threshold** |
| **Audio Thread Dynamic Allocations** | **0 Allocations** | **0 Allocations** | Bit-Exact Compliant |
| **Limiter Hard Ceiling** | **$\le 1.0000$ ($0\text{ dBFS}$)** | **$\le 1.0000$ ($0\text{ dBFS}$)** | Bounded Compliant |
| **Frequency Range Bounds** | **$[18.0, 350.0]\text{ Hz}$** | **$[18.0, 350.0]\text{ Hz}$** | Strictly Bounded |
| **Numerical Finite Validation** | **100% Finite** | **0 NaNs, 0 Infs, 0 Denormals** | Bitwise IEEE 754 Compliant |

---

## 5. Test Execution & Automation Instructions

### 5.1 Test Suite Location
All automated E2E test files and execution scripts reside under:
`c:\Users\x\Documents\antigravity\poopenfarten-pro\tests\e2e\`

- `test_framework.py`: Shared testing harness, mathematical DSP oracles, APVTS specifications, and assertion helpers.
- `tier1_feature_tests.py`: Tier 1 Feature Coverage test definitions (150 tests).
- `tier2_boundary_tests.py`: Tier 2 Boundary & Corner Case test definitions (150 tests).
- `tier3_interaction_tests.py`: Tier 3 Cross-Feature Pairwise Interaction test definitions (30 tests).
- `tier4_production_tests.py`: Tier 4 Real-World Application Scenarios (15 workloads).
- `run_e2e_tests.py`: Master automated test runner CLI.
- `run_e2e_tests.ps1`: Windows PowerShell execution wrapper.

### 5.2 Running the Full Automated E2E Test Suite

#### Via Python:
```powershell
# From project root:
python tests/e2e/run_e2e_tests.py

# With verbose diagnostic reporting:
python tests/e2e/run_e2e_tests.py --verbose

# Run a specific tier only:
python tests/e2e/run_e2e_tests.py --tier 1
python tests/e2e/run_e2e_tests.py --tier 2
python tests/e2e/run_e2e_tests.py --tier 3
python tests/e2e/run_e2e_tests.py --tier 4

# Run tests for a specific feature:
python tests/e2e/run_e2e_tests.py --feature F01
```

#### Via PowerShell Wrapper:
```powershell
# Run all tests:
.\tests\e2e\run_e2e_tests.ps1

# Run with verbose output:
.\tests\e2e\run_e2e_tests.ps1 -Verbose
```

### 5.3 Exit Code Protocol
- **Exit Code 0**: 100% PASS across all executed test cases.
- **Exit Code 1**: One or more assertions failed (diagnostics printed to stdout/stderr).
- **Exit Code 2**: Execution configuration error or missing test suite file.
