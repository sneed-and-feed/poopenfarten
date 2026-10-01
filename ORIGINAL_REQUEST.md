# Original User Request

## 2026-10-01T04:58:13Z

Build **PPF-42 DYNAMICS** (codenamed *Poopenfarten Pro*), a professional-grade aeroacoustic physical modeling synthesizer simulating gastrointestinal relaxation oscillation, fluid dynamics, and cavity acoustics as native VST3, CLAP, and Standalone (.exe) desktop plugins for FL Studio.

Working directory: c:\Users\x\Documents\antigravity\poopenfarten-pro
Integrity mode: development

Reference Blueprint: C:\Users\x\.gemini\antigravity\brain\9ed3d846-acc6-41b4-8ca9-6a1f4e8949bf\PPF42_DYNAMICS_SPEC.md
Existing Codebases for reusable DSP/IPC patterns (do not carry over Braun branding/UI):
- c:\Users\x\Documents\antigravity\braun_as-42
- c:\Users\x\Documents\antigravity\braun_rb-26

---

## Agent Operational Directives
- **No Tight Loop Micropolling:** Subagents and tasks must NOT micropoll in a tight loop or busy-wait on tasks/status. Rely strictly on reactive completions and event notifications.
- **Audio Verification First:** Run verification through automated headless C++ tests before presenting the build.

---

## Requirements

### R1. Aeroacoustic Physical Modeling Core Engine
Implement a continuous mathematical physical modeling synthesis engine (C++20):
1. **Non-Linear Sphincter Relaxation Oscillator:** Mass-spring-damper valve model with Bernoulli pressure coupling, dynamic aperture closure, and frequency tracking (18 Hz - 350 Hz).
2. **Aeroacoustic Jet Turbulence:** Reynolds-scaled noise generator modulated by instantaneous aperture airflow velocity.
3. **Multiphase Viscosity & Fluid Splatter:** Minnaert resonance micro-bubble generator and Poisson-distributed transient droplet pops for realistic moisture simulation.
4. **Resonant Tract & Porcelain Convolver:** Intergluteal cleft boundary waveguide filter coupled to a zero-latency ceramic porcelain cavity impulse response convolver.
5. **Sub-Bass Reinforcement & Mastering FX:** Dedicated sub-bass sine layer, harmonic saturation drive, and brickwall peak limiter.
6. **Voice Modes:** Switchable Mono-Legato (with portamento glide and MPE pressure support), Polyphonic (up to 8 voices), and Unison Detune.

### R2. JUCE 8 CMake Desktop Targets
Construct a clean JUCE 8 CMake project producing:
1. PPF-42_Dynamics_Standalone.exe
2. PPF-42_Dynamics.vst3
3. PPF-42_Dynamics.clap (if CLAP extensions available, otherwise VST3 + Standalone)
Target Windows x64 with MSVC, ensuring thread-safe AudioProcessorValueTreeState (APVTS) parameter layout and sample-rate independence (44.1 kHz - 192 kHz).

### R3. Modern Studio Pro Vector WebView UI
Implement a sleek dark-slate vector user interface using JUCE 8's WebView integration (juce::WebBrowserComponent with Edge WebView2):
1. Precision modern aesthetic (Vital/FabFilter style with dark obsidian background #0d1117, subtle borders, cyan and amber neon accents).
2. Real-time 60 FPS Canvas-based visualizers: Aeroacoustic flow particle chamber and live spectral oscilloscope fed by lock-free telemetry.
3. Interactive performance controls: Dual performance macros (Gut Squeeze, Dietary Moisture), gut pressure envelope, and parameter sliders with bi-directional IPC.

### R4. Factory Presets
Bundle a curated set of 10 hyper-realistic anatomical presets in APVTS state format (e.g., Clean Continental Purr, High-Tension Squeaker, Viscous Multiphase Splatter, Visceral Sub-Rumble 18Hz, Wet Porcelain Slam).

### R5. Automated Headless C++ DSP Test Suite
Implement an independent headless test executable (ppf42_headless_dsp_tests.exe) that executes without audio hardware or GUI:
1. **Numerical Stability:** Verify zero NaNs, infinities, or denormal numbers across 100,000 processed samples across extreme parameter sweeps.
2. **Dynamic Range & Bounded Output:** Verify the peak limiter prevents hard clipping (> 0 dBFS) under max Colonic Drive.
3. **Thread Safety & Glide:** Stress-test concurrent APVTS parameter changes during active audio block rendering.

---

## Acceptance Criteria

### Sound Engine & Numerical Integrity
- [ ] ppf42_headless_dsp_tests.exe compiles and executes with 100% PASS on all tests.
- [ ] No denormals, NaNs, or memory leaks during audio processing.
- [ ] Physical modeling responds continuously to MIDI notes, velocity, pitch bend, and glide.

### Build & Platform Targets
- [ ] CMake configures and builds PPF-42_Dynamics_Standalone and PPF-42_Dynamics_vst3 successfully without compiler errors.
- [ ] Standalone .exe launches and renders audio output.
- [ ] VST3 bundle validates and loads in host environment.

### Interface & Telemetry
- [ ] Embedded WebView UI loads cleanly without missing asset 404s.
- [ ] 60 Hz visualizer frames stream via lock-free telemetry without blocking the audio thread.
- [ ] Parameter changes from the UI update DSP in real time, and host automation reflects on UI controls.

## Follow-up — 2026-10-01T05:37:58Z

Git upstream configured: The local directory `c:\Users\x\Documents\antigravity\poopenfarten-pro` is now initialized as a Git repository connected to origin `https://github.com/sneed-and-feed/poopenfarten` on branch `main` with upstream tracking set. A standard C++/JUCE .gitignore has been created so build directories remain untracked. All subagents may stage, commit, and push verification/release artifacts as appropriate.
