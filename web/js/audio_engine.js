/**
 * PPF-42 DYNAMICS · Web Audio DSP Engine
 * Pure client-side physical modeling aeroacoustic synthesizer.
 * Matches C++ DSP algorithms:
 *   - SphincterOscillator (Adachi/Fletcher lip-reed with Bernoulli suction & collision)
 *   - FluidNoiseEngine (Reynolds jet turbulence, Minnaert bubbles, Poisson splatter)
 *   - CleftWaveguide (Intergluteal boundary comb filter with Hermite interpolation)
 *   - PorcelainConvolver (4-model FIR chamber resonance)
 *   - BoundedSaturator (C^1 cubic Hermite soft-clipper & brickwall limiter)
 *   - Multi-mode Voicing (Mono Legato, Poly 8-Voice, Stereo Unison Detune)
 *   - Real-time 60 FPS Telemetry stream (Base64 waveform, pressure, velocity, aperture)
 */

(function (global) {
    'use strict';

    // =========================================================================
    // Physical Constants & DSP Helpers
    // =========================================================================
    const kAirDensityRho = 1.204;     // kg/m^3 (ambient air at 20°C)
    const kSlitWidthW = 0.025;         // 25 mm anatomical slit width
    const kMinOscFrequencyHz = 18.0;   // Infrasonic visceral rumble
    const kMaxOscFrequencyHz = 350.0;  // High-tension squeaker
    const kDefaultFrequencyHz = 110.0; // A2 note
    const kTwoPi = 6.283185307179586;
    const kTelemetryScopeSamples = 128;

    function clamp(val, min, max) {
        return Math.max(min, Math.min(max, val));
    }

    function flushDenormal(val) {
        if (!Number.isFinite(val) || Math.abs(val) < 1.0e-15) return 0.0;
        return val;
    }

    function dbToGain(db) {
        return Math.pow(10.0, db * 0.05);
    }

    function noteToFreq(noteWithPitchBend) {
        const raw = 440.0 * Math.pow(2.0, (noteWithPitchBend - 69.0) / 12.0);
        return clamp(raw, kMinOscFrequencyHz, kMaxOscFrequencyHz);
    }

    function interpolateHermite4P3O(ym1, y0, y1, y2, frac) {
        const c0 = y0;
        const c1 = 0.5 * (y1 - ym1);
        const c2 = ym1 - 2.5 * y0 + 2.0 * y1 - 0.5 * y2;
        const c3 = 0.5 * (y2 - ym1) + 1.5 * (y0 - y1);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    // =========================================================================
    // Fast PRNG: Mulberry32 (Deterministic, High Uniformity, Zero Allocations)
    // =========================================================================
    class FastPrng {
        constructor(seed = 0x504F4F50) {
            this.state = seed >>> 0;
        }

        setSeed(seed) {
            this.state = (seed || 0xDEADBEEF) >>> 0;
        }

        nextFloat01() {
            let t = (this.state += 0x6D2B79F5) >>> 0;
            t = Math.imul(t ^ (t >>> 15), t | 1);
            t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
            return ((t ^ (t >>> 14)) >>> 0) / 4294967296.0;
        }

        nextFloatSigned() {
            return this.nextFloat01() * 2.0 - 1.0;
        }
    }

    // =========================================================================
    // Filters: OnePoleLowpass, BiquadDirectForm2T, SubBassDcBlocker
    // =========================================================================
    class OnePoleLowpass {
        constructor() {
            this.z1 = 0.0;
            this.alpha = 1.0;
        }

        setCutoff(sampleRate, cutoffHz) {
            const nyquist = sampleRate * 0.499;
            const fc = clamp(cutoffHz, 10.0, nyquist);
            const omega = (kTwoPi * fc) / sampleRate;
            this.alpha = clamp(1.0 - Math.exp(-omega), 0.0001, 1.0);
        }

        reset(val = 0.0) {
            this.z1 = val;
        }

        process(input) {
            this.z1 += this.alpha * (input - this.z1);
            this.z1 = flushDenormal(this.z1);
            return this.z1;
        }
    }

    class BiquadDirectForm2T {
        constructor() {
            this.b0 = 1.0; this.b1 = 0.0; this.b2 = 0.0;
            this.a1 = 0.0; this.a2 = 0.0;
            this.s1 = 0.0; this.s2 = 0.0;
        }

        reset() {
            this.s1 = 0.0;
            this.s2 = 0.0;
        }

        setBandpass(sampleRate, centerHz, q) {
            const nyquist = sampleRate * 0.499;
            const f0 = clamp(centerHz, 20.0, nyquist);
            const safeQ = Math.max(0.1, q);
            const w0 = (kTwoPi * f0) / sampleRate;
            const cosw0 = Math.cos(w0);
            const sinw0 = Math.sin(w0);
            const alpha = sinw0 / (2.0 * safeQ);

            const a0 = 1.0 + alpha;
            const invA0 = 1.0 / a0;

            this.b0 = alpha * invA0;
            this.b1 = 0.0;
            this.b2 = -alpha * invA0;
            this.a1 = (-2.0 * cosw0) * invA0;
            this.a2 = (1.0 - alpha) * invA0;
        }

        process(input) {
            const out = this.b0 * input + this.s1;
            this.s1 = this.b1 * input - this.a1 * out + this.s2;
            this.s2 = this.b2 * input - this.a2 * out;
            this.s1 = flushDenormal(this.s1);
            this.s2 = flushDenormal(this.s2);
            return flushDenormal(out);
        }
    }

    class SubBassDcBlocker {
        constructor() {
            this.poleRadius = 0.9980;
            this.x1 = 0.0;
            this.y1 = 0.0;
        }

        prepare(sampleRate) {
            const fs = sampleRate > 100.0 ? sampleRate : 48000.0;
            const rRaw = 1.0 - (kTwoPi * 15.0 / fs);
            this.poleRadius = clamp(rRaw, 0.0, 0.99995);
            this.reset();
        }

        reset() {
            this.x1 = 0.0;
            this.y1 = 0.0;
        }

        processSample(input) {
            const x = flushDenormal(input);
            const y = x - this.x1 + this.poleRadius * this.y1;
            this.x1 = x;
            this.y1 = flushDenormal(y);
            return this.y1;
        }
    }

    // =========================================================================
    // SphincterOscillator: Adachi/Fletcher Relaxation Oscillator
    // =========================================================================
    class SphincterOscillator {
        constructor() {
            this.sampleRate = 48000.0;
            this.displacement = 0.001; // 1 mm initial aperture
            this.velocity = 0.0;
            this.airVelocity = 0.0;
            this.upstreamPressure = 0.0;
            this.prevVolumeFlow = 0.0;
            this.prng = new FastPrng(0x504F4F50);
            this.stuck = false;
            this.burstThreshold = 350.0;
            this.justOpened = false;
            this.openingTransientAmp = 0.0;
            this.impactSlapAmp = 0.0;
        }

        prepare(sampleRate) {
            this.sampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
            this.prng.setSeed(0x504F4F50);
            this.reset();
        }

        reset() {
            this.displacement = 0.001;
            this.velocity = 0.0;
            this.airVelocity = 0.0;
            this.upstreamPressure = 0.0;
            this.prevVolumeFlow = 0.0;
            this.stuck = false;
            this.burstThreshold = 350.0;
            this.justOpened = false;
            this.openingTransientAmp = 0.0;
            this.impactSlapAmp = 0.0;
        }

        processSample(p) {
            this.justOpened = false;
            this.openingTransientAmp = 0.0;

            if (p.pressure <= 0.0001) {
                const y0 = clamp(p.aperture * 0.002 + 0.0005, 0.0002, 0.004);
                this.displacement = flushDenormal(this.displacement + 0.05 * (y0 - this.displacement));
                this.velocity = 0.0;
                this.airVelocity = 0.0;
                this.upstreamPressure = 0.0;
                this.prevVolumeFlow = 0.0;
                this.stuck = false;
                this.impactSlapAmp = 0.0;
                return {
                    acousticWave: 0.0,
                    airVelocity: 0.0,
                    aperture: this.displacement,
                    justOpened: false,
                    openingTransientAmp: 0.0
                };
            }

            const dt = 1.0 / this.sampleRate;

            // 1. Clamped fundamental frequency and tension offset
            const tensionMultiplier = 0.85 + 0.30 * clamp(p.tension, 0.0, 1.0);
            const fTarget = clamp(p.frequencyHz * tensionMultiplier, kMinOscFrequencyHz, kMaxOscFrequencyHz);
            const kRelaxationCorrection = 1.28;
            const f0 = clamp(fTarget * kRelaxationCorrection, kMinOscFrequencyHz, kMaxOscFrequencyHz * 1.6);

            // 2. Viscous mass and damping loading
            const visc = clamp(p.viscosity, 0.0, 1.0);
            const mEff = 0.001 * (1.0 + 0.85 * visc);
            const omega0 = kTwoPi * f0;
            const k0 = mEff * omega0 * omega0;
            const r0 = 2.0 * 0.08 * omega0 * mEff * (1.0 + 1.50 * visc);

            // 3. Resting aperture y0 (meters)
            const y0 = clamp(p.aperture * 0.002 + 0.0005, 0.0002, 0.004);

            // 4. Upstream colonic pressure head (Pa)
            const pDrive = p.pressure * 1500.0;
            const fillRate = 3.5 * omega0;
            const currentArea = this.stuck ? 0.0 : (kSlitWidthW * Math.max(0.0, this.displacement));
            const flowVolume = currentArea * this.airVelocity;

            const dPup = fillRate * (pDrive - this.upstreamPressure) - (flowVolume * omega0 * 8.0);
            this.upstreamPressure = clamp(this.upstreamPressure + dt * dPup, 0.0, 3000.0);
            this.upstreamPressure = flushDenormal(this.upstreamPressure);

            // 5. Mucosal Stiction & Seal Burst Logic
            if (this.stuck) {
                if (this.upstreamPressure >= this.burstThreshold || this.upstreamPressure >= (0.96 * pDrive)) {
                    this.stuck = false;
                    this.justOpened = true;
                    const burstVel = Math.sqrt(Math.max(0.0, 2.0 * this.upstreamPressure / mEff)) * 0.035 + 1.8;
                    this.velocity = burstVel;
                    this.openingTransientAmp = burstVel;
                    this.displacement = 0.00015;
                } else {
                    this.displacement = 0.0;
                    this.velocity = 0.0;
                    this.airVelocity = 0.0;
                }
            }

            // 6. Airflow velocity through aperture via Bernoulli equation
            if (!this.stuck && this.displacement > 1.0e-6 && this.upstreamPressure > 0.0) {
                this.airVelocity = Math.sqrt(2.0 * this.upstreamPressure / kAirDensityRho);
            } else {
                this.airVelocity = 0.0;
            }
            this.airVelocity = flushDenormal(this.airVelocity);

            // 7. Flutter / Asymmetric tissue chaos
            if (!this.stuck) {
                const flutterAmt = clamp(p.flutter, 0.0, 1.0);
                const dispOffset = this.displacement - y0;
                const kFlutter = k0 * (1.0 + 0.4 * flutterAmt * (dispOffset * dispOffset * 1.0e6));
                const rFlutter = r0 * (1.0 - 0.25 * flutterAmt * (this.velocity > 0.0 ? 1.0 : -1.0));

                const springForce = -kFlutter * dispOffset;
                const dampingForce = -rFlutter * this.velocity;
                const tissueArea = 0.00025;
                const freqRatio = f0 / 110.0;
                const forceScaling = clamp(freqRatio * freqRatio, 0.02, 15.0);

                const gapRatio = y0 / Math.max(0.0001, this.displacement);
                const dynamicPressure = 0.5 * kAirDensityRho * this.airVelocity * this.airVelocity * gapRatio;
                const drivingForce = tissueArea * (this.upstreamPressure - dynamicPressure) * forceScaling;

                const totalForce = springForce + dampingForce + drivingForce;
                const acceleration = totalForce / mEff;

                this.velocity += dt * acceleration;
                this.velocity = clamp(this.velocity, -25.0, 25.0);
                this.velocity = flushDenormal(this.velocity);

                this.displacement += dt * this.velocity;

                // 8. Boundary contact stiffness and mucosal stiction capture at y = 0
                if (this.displacement <= 0.0) {
                    this.displacement = 0.0;
                    this.airVelocity = 0.0;
                    this.stuck = true;

                    this.impactSlapAmp = clamp(-this.velocity * 0.35, 0.0, 6.0);
                    this.velocity = 0.0;

                    const pThreshold = pDrive * (0.35 + 0.22 * clamp(p.tension, 0.0, 1.0));
                    const jitter = this.prng.nextFloatSigned() * 0.35;
                    this.burstThreshold = Math.max(40.0, pThreshold * (1.0 + jitter));
                }
            }
            this.displacement = flushDenormal(this.displacement);

            // 9. Radiated acoustic signal
            const currentVolumeFlow = kSlitWidthW * this.displacement * this.airVelocity;
            const dVolumeFlowDt = (currentVolumeFlow - this.prevVolumeFlow) * dt * 1000.0;
            this.prevVolumeFlow = currentVolumeFlow;

            const openPulse = this.justOpened ? (this.openingTransientAmp * 0.12) : 0.0;
            const slapPulse = (this.impactSlapAmp > 0.0) ? (this.impactSlapAmp * -0.08) : 0.0;
            this.impactSlapAmp *= 0.85;
            if (this.impactSlapAmp < 1.0e-3) this.impactSlapAmp = 0.0;

            const acousticWave = (this.displacement - y0) * 800.0 + dVolumeFlowDt * 0.10 + openPulse + slapPulse;
            return {
                acousticWave: flushDenormal(acousticWave),
                airVelocity: this.airVelocity,
                aperture: this.displacement,
                justOpened: this.justOpened,
                openingTransientAmp: this.openingTransientAmp
            };
        }
    }

    // =========================================================================
    // FluidNoiseEngine: Turbulence, Minnaert Bubbles & Poisson Splatter
    // =========================================================================
    class FluidNoiseEngine {
        constructor() {
            this.sampleRate = 48000.0;
            this.prng = new FastPrng(0x504F4F50);

            this.pinkB0 = 0.0;
            this.pinkB1 = 0.0;
            this.pinkB2 = 0.0;

            this.bubbleVoices = [];
            for (let i = 0; i < 16; ++i) {
                this.bubbleVoices.push({
                    active: false,
                    phase: 0.0,
                    phaseInc: 0.0,
                    envelope: 0.0,
                    decayRate: 0.98,
                    frequencyHz: 1500.0
                });
            }
            this.bubbleSpawnCounter = 200;

            this.samplesUntilNextDroplet = 2000;
            this.dropletFilters = [new BiquadDirectForm2T(), new BiquadDirectForm2T(), new BiquadDirectForm2T(), new BiquadDirectForm2T()];
            this.dropletImpulses = [0.0, 0.0, 0.0, 0.0];
            this.dropletFilterIdx = 0;
            this.lastDropletTrigger = 0.0;

            this.gurgleFilter = new OnePoleLowpass();
            this.viscosityFilter = new OnePoleLowpass();
            this.activeBubbleActivity = 0.0;
        }

        prepare(sampleRate) {
            this.sampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
            this.prng.setSeed(0x504F4F50);
            this.reset();
        }

        reset() {
            this.pinkB0 = 0.0;
            this.pinkB1 = 0.0;
            this.pinkB2 = 0.0;

            for (const v of this.bubbleVoices) {
                v.active = false;
                v.phase = 0.0;
                v.envelope = 0.0;
            }
            this.bubbleSpawnCounter = 200;
            this.samplesUntilNextDroplet = Math.floor(this.sampleRate * 0.05);
            for (const f of this.dropletFilters) {
                f.reset();
            }
            this.dropletImpulses = [0.0, 0.0, 0.0, 0.0];
            this.dropletFilterIdx = 0;
            this.lastDropletTrigger = 0.0;
            this.gurgleFilter.reset();
            this.gurgleFilter.setCutoff(this.sampleRate, 750.0);
            this.viscosityFilter.reset();
            this.activeBubbleActivity = 0.0;
        }

        triggerBubble(radiusMm, intensity) {
            const rClamped = clamp(radiusMm, 0.5, 4.0) * 0.001;
            const freq = clamp(3.28325 / rClamped, 800.0, 6500.0);
            const nyquist = this.sampleRate * 0.49;
            const safeFreq = Math.min(freq, nyquist);

            let bestIdx = 0;
            let lowestEnv = 1.0e10;
            for (let i = 0; i < this.bubbleVoices.length; ++i) {
                if (!this.bubbleVoices[i].active) {
                    bestIdx = i;
                    break;
                }
                if (this.bubbleVoices[i].envelope < lowestEnv) {
                    lowestEnv = this.bubbleVoices[i].envelope;
                    bestIdx = i;
                }
            }

            const v = this.bubbleVoices[bestIdx];
            v.active = true;
            v.frequencyHz = safeFreq;
            v.phase = this.prng.nextFloat01() * kTwoPi;
            v.phaseInc = safeFreq * (kTwoPi / this.sampleRate);
            v.envelope = clamp(intensity, 0.01, 1.0);

            const decayTimeSec = 0.012 + 0.015 * (rClamped * 1000.0 / 4.0);
            v.decayRate = Math.exp(-1.0 / (decayTimeSec * this.sampleRate));
        }

        triggerDroplet(intensity) {
            const popFreq = 1500.0 + this.prng.nextFloat01() * 3000.0;
            const popQ = 8.0 + this.prng.nextFloat01() * 10.0;
            const filter = this.dropletFilters[this.dropletFilterIdx];
            filter.setBandpass(this.sampleRate, popFreq, popQ);
            this.dropletImpulses[this.dropletFilterIdx] = intensity;
            this.lastDropletTrigger = intensity;
            this.dropletFilterIdx = (this.dropletFilterIdx + 1) % this.dropletFilters.length;
        }

        triggerFlapBurst(openingIntensity, moisture, dropletRate) {
            if (moisture <= 0.0001) return;

            const numDrops = clamp(1 + Math.floor(moisture * (1.0 + dropletRate * 2.5)), 1, this.dropletFilters.length);
            for (let i = 0; i < numDrops; ++i) {
                const popFreq = 1500.0 + this.prng.nextFloat01() * 3000.0;
                const popQ = 8.0 + this.prng.nextFloat01() * 10.0;
                const intensity = openingIntensity * moisture * (0.35 + 0.65 * this.prng.nextFloat01()) * 0.45;

                const filter = this.dropletFilters[this.dropletFilterIdx];
                filter.setBandpass(this.sampleRate, popFreq, popQ);
                this.dropletImpulses[this.dropletFilterIdx] = intensity;
                if (intensity > this.lastDropletTrigger) {
                    this.lastDropletTrigger = intensity;
                }
                this.dropletFilterIdx = (this.dropletFilterIdx + 1) % this.dropletFilters.length;
            }

            const numBubbles = (this.prng.nextFloat01() < (moisture * 0.85)) ? 2 : 1;
            for (let i = 0; i < numBubbles; ++i) {
                const rMm = 0.6 + this.prng.nextFloat01() * 2.5;
                const bubbleInt = moisture * (0.3 + 0.7 * this.prng.nextFloat01()) * (0.3 + 0.2 * openingIntensity);
                this.triggerBubble(rMm, bubbleInt);
            }
        }

        processSample(airVelocity, aperture, p) {
            this.lastDropletTrigger = 0.0;

            // 1. Reynolds Jet Turbulence Noise
            let turbSignal = 0.0;
            const posAirVel = Math.max(0.0, airVelocity);
            const posAperture = Math.max(0.0, aperture);

            if (posAperture > 1.0e-6 && posAirVel > 0.01) {
                const white = this.prng.nextFloatSigned();
                this.pinkB0 = 0.99765 * this.pinkB0 + white * 0.0990460;
                this.pinkB1 = 0.96300 * this.pinkB1 + white * 0.2965164;
                this.pinkB2 = 0.57000 * this.pinkB2 + white * 1.0526913;
                const pink = this.pinkB0 + this.pinkB1 + this.pinkB2 + white * 0.1848;

                const reynoldsScaling = Math.pow(posAirVel / 20.0, 2.5);
                const effectiveArea = (kSlitWidthW * posAperture) * 100.0;
                turbSignal = pink * reynoldsScaling * effectiveArea * 0.12;

                const moisture = clamp(p.moisture, 0.0, 1.0);
                if (moisture > 0.0001) {
                    const microNoise = this.prng.nextFloatSigned();
                    const smoothNoise = this.gurgleFilter.process(microNoise);
                    turbSignal *= (1.0 + 0.40 * moisture * smoothNoise);
                }

                turbSignal = flushDenormal(turbSignal);
            }

            // 2. Multiphase Fluid Engine (Minnaert Bubbles & Poisson Droplets)
            let fluidSignal = 0.0;
            const moisture = clamp(p.moisture, 0.0, 1.0);

            if (moisture > 0.0001 && posAirVel > 0.1) {
                if (--this.bubbleSpawnCounter <= 0) {
                    const spawnInterval = (0.005 + (1.0 - moisture) * 0.040) * this.sampleRate;
                    this.bubbleSpawnCounter = Math.max(16, Math.floor(spawnInterval * (0.8 + 0.4 * this.prng.nextFloat01())));
                    const rMm = 0.5 + this.prng.nextFloat01() * 3.5;
                    const intensity = moisture * (0.2 + 0.8 * this.prng.nextFloat01()) * (posAirVel / 25.0);
                    this.triggerBubble(rMm, intensity);
                }

                if (--this.samplesUntilNextDroplet <= 0) {
                    const lambda = Math.max(0.1, (1.0 + p.droplet_rate * 39.0) * moisture * (posAirVel / 15.0));
                    const u = clamp(this.prng.nextFloat01(), 1.0e-5, 0.99999);
                    const intervalSec = -Math.log(1.0 - u) / lambda;
                    this.samplesUntilNextDroplet = Math.max(12, Math.floor(intervalSec * this.sampleRate));
                    const dropIntensity = moisture * (0.3 + 0.7 * this.prng.nextFloat01());
                    this.triggerDroplet(dropIntensity);
                }
            }

            // Render active bubble voices
            let bubbleSum = 0.0;
            let activeEnvs = 0.0;
            for (const v of this.bubbleVoices) {
                if (!v.active) continue;

                v.phase += v.phaseInc;
                if (v.phase >= kTwoPi) v.phase -= kTwoPi;

                bubbleSum += Math.sin(v.phase) * v.envelope;
                activeEnvs += v.envelope;

                v.envelope *= v.decayRate;
                if (v.envelope < 1.0e-4) {
                    v.active = false;
                    v.envelope = 0.0;
                }
            }
            this.activeBubbleActivity = clamp(activeEnvs * 0.2, 0.0, 1.0);

            // Render all droplet pop filters
            let dropSample = 0.0;
            for (let i = 0; i < this.dropletFilters.length; ++i) {
                dropSample += this.dropletFilters[i].process(this.dropletImpulses[i]);
                this.dropletImpulses[i] = 0.0;
            }

            fluidSignal = (bubbleSum * 0.35 + dropSample * 0.55) * moisture;

            // 3. Sum and Viscosity Filtering
            const rawOutput = turbSignal + fluidSignal;
            const visc = clamp(p.viscosity, 0.0, 1.0);
            const viscCutoff = 14000.0 - visc * 11500.0;
            this.viscosityFilter.setCutoff(this.sampleRate, viscCutoff);

            return this.viscosityFilter.process(rawOutput);
        }

        getBubbleActivity() { return this.activeBubbleActivity; }
        getDropletPopTrigger() { return this.lastDropletTrigger; }
    }

    // =========================================================================
    // PressureEnvelope: ADSR Pressure Modulator
    // =========================================================================
    class PressureEnvelope {
        constructor() {
            this.sampleRate = 48000.0;
            this.currentLevel = 0.0;
            this.velocity = 1.0;
            this.stage = 'Idle'; // 'Idle', 'Attack', 'Decay', 'Sustain', 'Release'
        }

        prepare(sampleRate) {
            this.sampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
            this.reset();
        }

        reset() {
            this.stage = 'Idle';
            this.currentLevel = 0.0;
        }

        noteOn(velocity) {
            this.velocity = clamp(velocity, 0.01, 1.0);
            this.stage = 'Attack';
        }

        noteOff() {
            if (this.stage !== 'Idle') {
                this.stage = 'Release';
            }
        }

        isActive() {
            return this.stage !== 'Idle';
        }

        process(attackMs, decayMs, sustain, releaseMs) {
            const dt = 1.0 / this.sampleRate;
            switch (this.stage) {
                case 'Idle':
                    this.currentLevel = 0.0;
                    break;

                case 'Attack': {
                    const aTime = Math.max(0.0001, attackMs * 0.001);
                    this.currentLevel += dt / aTime;
                    if (this.currentLevel >= 1.0) {
                        this.currentLevel = 1.0;
                        this.stage = 'Decay';
                    }
                    break;
                }

                case 'Decay': {
                    const dTime = Math.max(0.001, decayMs * 0.001);
                    const target = clamp(sustain, 0.0, 1.0);
                    const rate = 1.0 - Math.exp(-dt / (dTime * 0.35));
                    this.currentLevel += rate * (target - this.currentLevel);
                    if (Math.abs(this.currentLevel - target) < 0.005) {
                        this.currentLevel = target;
                        this.stage = 'Sustain';
                    }
                    break;
                }

                case 'Sustain':
                    this.currentLevel = clamp(sustain, 0.0, 1.0);
                    break;

                case 'Release': {
                    const rTime = Math.max(0.001, releaseMs * 0.001);
                    const rate = 1.0 - Math.exp(-dt / (rTime * 0.35));
                    this.currentLevel -= rate * this.currentLevel;
                    if (this.currentLevel < 1.0e-4) {
                        this.currentLevel = 0.0;
                        this.stage = 'Idle';
                    }
                    break;
                }
            }

            this.currentLevel = flushDenormal(this.currentLevel);
            return this.currentLevel;
        }
    }

    // =========================================================================
    // SamplePitcherEngine: Catmull-Rom Hermite Resampling Engine
    // =========================================================================
    class SamplePitcherEngine {
        constructor() {
            this.sampleRate = 48000.0;
            this.playhead = 0.0;
            this.active = false;
            this.finished = true;
            this.currentSampleIndex = 0;
            this.sampleLength = 0;
            this.sampleData = null;
            this.reverse = false;
        }

        prepare(sampleRate) {
            this.sampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
            this.reset();
        }

        reset() {
            this.playhead = 0.0;
            this.active = false;
            this.finished = true;
            this.currentSampleIndex = 0;
            this.sampleLength = 0;
            this.sampleData = null;
            this.reverse = false;
        }

        noteOn(sampleIndex = 0, startOffset01 = 0.0, reverse = false, customData = null) {
            this.reverse = !!reverse;
            if (customData && customData.length > 0) {
                this.sampleData = customData;
                this.sampleLength = customData.length;
            } else if (global.SampleBank) {
                const count = global.SampleBank.getSampleCount();
                const idx = Math.max(0, Math.min(count - 1, sampleIndex || 0));
                this.currentSampleIndex = idx;
                this.sampleData = global.SampleBank.getFloatData(idx);
                this.sampleLength = this.sampleData ? this.sampleData.length : 0;
            }

            this.finished = (!this.sampleData || this.sampleLength === 0);
            if (this.finished) {
                this.active = false;
                this.playhead = 0.0;
                return;
            }

            this.active = true;
            const clampedOffset = clamp(startOffset01, 0.0, 0.999);
            const maxIdx = this.sampleLength - 1;
            this.playhead = this.reverse ? (1.0 - clampedOffset) * maxIdx : clampedOffset * maxIdx;
        }

        noteOff() {
            // One-shot finishes with envelope
        }

        isActive() {
            return this.active && !this.finished;
        }

        processSample(effectiveFreq) {
            if (!this.active || this.finished || !this.sampleData || this.sampleLength === 0) {
                return { signal: 0.0, airVelocity: 0.0, aperture: 0.0 };
            }

            if (!this.reverse) {
                if (this.playhead >= this.sampleLength - 1) {
                    this.finished = true;
                    return { signal: 0.0, airVelocity: 0.0, aperture: 0.0 };
                }
            } else {
                if (this.playhead <= 0.0) {
                    this.finished = true;
                    return { signal: 0.0, airVelocity: 0.0, aperture: 0.0 };
                }
            }

            const i0 = Math.floor(this.playhead);
            const frac = this.playhead - i0;
            const len = this.sampleLength;

            const im1 = Math.max(0, Math.min(len - 1, i0 - 1));
            const i_0 = Math.max(0, Math.min(len - 1, i0));
            const i1  = Math.max(0, Math.min(len - 1, i0 + 1));
            const i2  = Math.max(0, Math.min(len - 1, i0 + 2));

            const ym1 = this.sampleData[im1];
            const y0  = this.sampleData[i_0];
            const y1  = this.sampleData[i1];
            const y2  = this.sampleData[i2];

            const wave = interpolateHermite4P3O(ym1, y0, y1, y2, frac);

            const absWave = Math.abs(wave);
            const airVel = absWave * 32.0;
            const aperture = 0.001 + 0.0035 * absWave;

            // Pitch step relative to MIDI 48 (C3 = 130.8128 Hz)
            const safeFreq = clamp(effectiveFreq, kMinOscFrequencyHz, kMaxOscFrequencyHz);
            const pitchRatio = safeFreq / 130.8127826503;
            const rateStep = pitchRatio * (48000.0 / this.sampleRate);

            if (!this.reverse) {
                this.playhead += rateStep;
                if (this.playhead >= len - 1) this.finished = true;
            } else {
                this.playhead -= rateStep;
                if (this.playhead <= 0.0) this.finished = true;
            }

            return {
                signal: flushDenormal(wave),
                airVelocity: airVel,
                aperture: aperture
            };
        }
    }

    // =========================================================================
    // PhysicalVoice: Single Aeroacoustic Modeling Voice
    // =========================================================================
    class PhysicalVoice {
        constructor() {
            this.sampleRate = 48000.0;
            this.noteNumber = -1;
            this.velocity = 0.0;
            this.pitchBendSemi = 0.0;
            this.aftertouch = 0.0;
            this.pedalLatched = false;
            this.ageSamples = 0;

            this.currentFrequency = 110.0;
            this.targetFrequency = 110.0;
            this.glideCoeff = 1.0;

            this.declickGain = 1.0;
            this.declickDelta = 0.0;

            this.pressureVented = 0.0;
            this.squelchFilter = new BiquadDirectForm2T();
            this.subThumpFilter = new OnePoleLowpass();

            this.samplePitcher = new SamplePitcherEngine();
            this.fluid = new FluidNoiseEngine();
            this.envelope = new PressureEnvelope();
        }

        prepare(sampleRate) {
            this.sampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
            this.samplePitcher.prepare(sampleRate);
            this.fluid.prepare(sampleRate);
            this.envelope.prepare(sampleRate);
            this.reset();
        }

        reset() {
            this.noteNumber = -1;
            this.velocity = 0.0;
            this.pitchBendSemi = 0.0;
            this.aftertouch = 0.0;
            this.pedalLatched = false;
            this.ageSamples = 0;
            this.currentFrequency = 110.0;
            this.targetFrequency = 110.0;
            this.glideCoeff = 1.0;
            this.declickGain = 1.0;
            this.declickDelta = 0.0;
            this.pressureVented = 0.0;
            this.squelchFilter.reset();
            this.subThumpFilter.reset();

            this.samplePitcher.reset();
            this.fluid.reset();
            this.envelope.reset();
        }

        noteOn(noteNumber, velocity, initialGlideFreq = 0.0, sampleIndex = 0, sampleStart = 0.0, sampleReverse = false, customData = null) {
            const wasActive = this.isActive();
            this.noteNumber = noteNumber;
            this.velocity = clamp(velocity, 0.01, 1.0);
            this.pedalLatched = false;
            this.ageSamples = 0;

            if (wasActive) {
                const fadeSamples = Math.max(16.0, 0.005 * this.sampleRate);
                this.declickGain = 0.0;
                this.declickDelta = 1.0 / fadeSamples;
            } else {
                this.declickGain = 1.0;
                this.declickDelta = 0.0;
            }

            const targetFreq = noteToFreq(noteNumber + this.pitchBendSemi);
            this.targetFrequency = targetFreq;

            if (initialGlideFreq > 1.0) {
                this.currentFrequency = initialGlideFreq;
            } else {
                this.currentFrequency = targetFreq;
            }

            this.pressureVented = 0.0;
            this.squelchFilter.reset();
            this.subThumpFilter.reset();

            this.samplePitcher.noteOn(sampleIndex, sampleStart, sampleReverse, customData);
            this.envelope.noteOn(this.velocity);
        }

        noteOff() {
            if (!this.pedalLatched) {
                this.envelope.noteOff();
                this.samplePitcher.noteOff();
            }
        }

        setPitchBend(semitones) {
            this.pitchBendSemi = semitones;
            if (this.noteNumber >= 0) {
                this.targetFrequency = noteToFreq(this.noteNumber + this.pitchBendSemi);
            }
        }

        setAftertouch(pressure) {
            this.aftertouch = clamp(pressure, 0.0, 1.0);
        }

        setFrequencyTarget(targetHz, glideTimeMs) {
            this.targetFrequency = clamp(targetHz, kMinOscFrequencyHz, kMaxOscFrequencyHz);
            if (glideTimeMs <= 0.5) {
                this.currentFrequency = this.targetFrequency;
                this.glideCoeff = 1.0;
            } else {
                const timeSec = glideTimeMs * 0.001;
                this.glideCoeff = 1.0 - Math.exp(-1.0 / (this.sampleRate * timeSec * 0.35));
            }
        }

        isActive() {
            return this.envelope.isActive() || (this.declickDelta < 0.0 && this.declickGain > 1.0e-4);
        }

        isReleased() {
            return this.envelope.stage === 'Release';
        }

        processSample(params) {
            if (!this.isActive()) {
                return {
                    signal: 0.0,
                    airVelocity: 0.0,
                    aperture: 0.0,
                    bubbleActivity: 0.0,
                    dropletPop: 0.0
                };
            }

            this.ageSamples++;

            // 1. Portamento glide
            this.currentFrequency += this.glideCoeff * (this.targetFrequency - this.currentFrequency);
            this.currentFrequency = flushDenormal(this.currentFrequency);

            // 2. Colonic pressure head
            const envLevel = this.envelope.process(params.env_attack, params.env_decay, params.env_sustain, params.env_release);
            const velScale = 0.30 + 0.70 * (this.velocity * this.velocity);
            const aftertouchBoost = 1.0 + 0.50 * this.aftertouch;
            const effPressure = clamp(params.pressure * velScale * envLevel * aftertouchBoost, 0.0, 1.0);

            const isRelease = (this.envelope.stage === 'Release');

            // Dynamic pressure venting droop: emissions naturally decelerate 30-50% over envelope
            const ventRate = (effPressure * 1.5 + 0.2) / (this.sampleRate * Math.max(0.12, (params.env_decay + params.env_release) * 0.001));
            this.pressureVented = Math.min(1.0, this.pressureVented + ventRate);

            const droopFactor = 1.0 - 0.38 * Math.pow(this.pressureVented, 0.7);
            const releaseDroop = isRelease ? (0.75 * (0.35 + 0.65 * envLevel)) : 1.0;
            const effectiveFreq = clamp(this.currentFrequency * droopFactor * releaseDroop, kMinOscFrequencyHz, kMaxOscFrequencyHz);

            // 3. Resample authentic audio recording with Catmull-Rom Hermite interpolation
            const sampleRes = this.samplePitcher.processSample(effectiveFreq);
            const sampleWave = sampleRes.signal;

            // Modulate aeroacoustic airflow and aperture by pressure head
            const airVel = sampleRes.airVelocity * effPressure;
            const aperture = clamp(sampleRes.aperture * (0.4 + 0.6 * effPressure), 0.0001, 0.01);

            // 4. Fluid turbulence and bubbles
            const fluidP = {
                viscosity: params.viscosity,
                moisture: params.moisture,
                droplet_rate: params.droplet_rate
            };

            // Trigger explosive droplet clicks and bubble chirps on burst transients
            if (Math.abs(sampleWave) > 0.45 && (this.ageSamples % 480 === 0)) {
                this.fluid.triggerFlapBurst(Math.abs(sampleWave), params.moisture, params.droplet_rate);
            }

            const fluidWave = this.fluid.processSample(airVel, aperture, fluidP);

            // 5. Dynamic Wet Squelch Formant Filter (650 Hz - 1200 Hz, Q ~ 4.5 - 6.0)
            const apertureNorm = clamp(aperture / 0.003, 0.0, 1.0);
            const fSquelch = 650.0 + 550.0 * apertureNorm;
            const qSquelch = 4.5 + 1.5 * clamp(params.moisture, 0.0, 1.0);
            this.squelchFilter.setBandpass(this.sampleRate, fSquelch, qSquelch);

            // Route sample and fluid turbulence through wet squelch formant
            const squelchInput = sampleWave + fluidWave * 0.75;
            const squelchOut = this.squelchFilter.process(squelchInput);

            const moistureAmt = clamp(params.moisture, 0.0, 1.0);
            const squelchMix = moistureAmt * moistureAmt;
            const wetAcousticSignal = sampleWave + fluidWave * (0.35 * moistureAmt) + squelchOut * (0.75 * squelchMix);

            // 6. Asymmetric, soft-saturated aerodynamic volume velocity displacement pulse
            const subGain = dbToGain(params.sub_level);
            const flowThump = (aperture * 1000.0) * (airVel / 25.0);
            const dispThump = (aperture * 1000.0) * 0.85;
            const rawThump = flowThump * 0.65 + dispThump * 0.35;

            const satThump = (rawThump > 0.0) ? (rawThump / (1.0 + 0.75 * rawThump)) : (rawThump * 0.6);

            const subCutoff = clamp(effectiveFreq * 1.35, 32.0, 95.0);
            this.subThumpFilter.setCutoff(this.sampleRate, subCutoff);
            const visceralThump = this.subThumpFilter.process(satThump) * 1.5;
            const subBassWave = visceralThump * subGain * envLevel;

            // 7. Voice summation shaped by dynamic pressure envelope
            let voiceOutput = (wetAcousticSignal + subBassWave) * envLevel;

            // 8. De-click crossfading
            if (this.declickDelta > 0.0) {
                this.declickGain += this.declickDelta;
                if (this.declickGain >= 1.0) {
                    this.declickGain = 1.0;
                    this.declickDelta = 0.0;
                }
                voiceOutput *= this.declickGain;
            } else if (this.declickDelta < 0.0) {
                this.declickGain += this.declickDelta;
                if (this.declickGain <= 0.0) {
                    this.declickGain = 0.0;
                    this.envelope.reset();
                }
                voiceOutput *= this.declickGain;
            }

            return {
                signal: flushDenormal(voiceOutput),
                airVelocity: oscRes.airVelocity,
                aperture: oscRes.aperture,
                bubbleActivity: this.fluid.getBubbleActivity(),
                dropletPop: this.fluid.getDropletPopTrigger()
            };
        }
    }

    // =========================================================================
    // CleftWaveguide: Resonant Intergluteal Cleft Waveguide
    // =========================================================================
    class CleftWaveguide {
        constructor() {
            this.kMaxDelaySamples = 2048;
            this.kDelayMask = this.kMaxDelaySamples - 1;
            this.sampleRate = 48000.0;
            this.delayLine = new Float32Array(this.kMaxDelaySamples);
            this.writeIdx = 0;
            this.dampingZ1 = 0.0;
            this.resonanceEnergy = 0.0;
        }

        prepare(sampleRate) {
            this.sampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
            this.reset();
        }

        reset() {
            this.delayLine.fill(0.0);
            this.writeIdx = 0;
            this.dampingZ1 = 0.0;
            this.resonanceEnergy = 0.0;
        }

        processSample(input, damping) {
            const safeDamping = clamp(damping, 0.0, 1.0);

            // Anatomical cleft boundary delay: ~1.2 ms
            const delaySamples = clamp(0.0012 * this.sampleRate, 4.0, this.kMaxDelaySamples - 8);
            const readPos = this.writeIdx - delaySamples;

            const intPart = Math.floor(readPos);
            const frac = readPos - intPart;

            const idx0 = (intPart - 1) & this.kDelayMask;
            const idx1 = intPart & this.kDelayMask;
            const idx2 = (intPart + 1) & this.kDelayMask;
            const idx3 = (intPart + 2) & this.kDelayMask;

            const ym1 = this.delayLine[idx0];
            const y0 = this.delayLine[idx1];
            const y1 = this.delayLine[idx2];
            const y2 = this.delayLine[idx3];

            const delayed = interpolateHermite4P3O(ym1, y0, y1, y2, frac);

            // High-frequency boundary absorption filter
            const dampCutoff = 0.20 + 0.60 * safeDamping;
            this.dampingZ1 += (1.0 - dampCutoff) * (delayed - this.dampingZ1);
            this.dampingZ1 = flushDenormal(this.dampingZ1);

            // Feedback comb gain: safe smooth boundary reflection
            const gCleft = clamp(0.35 * (1.0 - 0.50 * safeDamping), 0.0, 0.45);
            const feedback = gCleft * this.dampingZ1;

            const toDelay = flushDenormal(input + feedback);
            this.delayLine[this.writeIdx] = toDelay;
            this.writeIdx = (this.writeIdx + 1) & this.kDelayMask;

            const out = flushDenormal(input + feedback * 0.40);

            // Track smoothed resonance energy for visualizer telemetry
            const sampleEnergy = out * out;
            this.resonanceEnergy = flushDenormal(0.99 * this.resonanceEnergy + 0.01 * sampleEnergy);

            return out;
        }

        getResonanceEnergy() {
            return this.resonanceEnergy;
        }
    }

    // =========================================================================
    // PorcelainConvolver: Ceramic Chamber Impulse Convolver
    // =========================================================================
    class PorcelainConvolver {
        constructor() {
            this.kKernelSize = 1024;
            this.kKernelMask = this.kKernelSize - 1;
            this.sampleRate = 48000.0;
            this.historyBuffer = new Float32Array(this.kKernelSize);
            this.kernel = new Float32Array(this.kKernelSize);
            this.writeIdx = 0;
            this.currentModel = -1;
            this.currentSize = -1.0;
        }

        prepare(sampleRate) {
            this.sampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
            this.currentModel = -1;
            this.currentSize = -1.0;
            this.reset();
        }

        reset() {
            this.historyBuffer.fill(0.0);
            this.writeIdx = 0;
            this.rebuildKernel(this.currentModel >= 0 ? this.currentModel : 1, this.currentSize > 0 ? this.currentSize : 1.0);
        }

        rebuildKernel(model, size) {
            this.currentModel = clamp(Math.round(model), 0, 3);
            this.currentSize = clamp(size, 0.5, 2.0);

            this.kernel.fill(0.0);
            const s = this.currentSize;
            const dt = 1.0 / this.sampleRate;

            switch (this.currentModel) {
                case 0: { // Dry Chamber
                    this.kernel[0] = 0.90;
                    const d1 = clamp(Math.floor(0.0015 * this.sampleRate * s), 1, this.kKernelSize - 1);
                    const d2 = clamp(Math.floor(0.0030 * this.sampleRate * s), 1, this.kKernelSize - 1);
                    this.kernel[d1] = -0.15;
                    this.kernel[d2] = 0.08;
                    break;
                }

                case 1: { // Ceramic Bowl - Standard
                    this.kernel[0] = 0.70;
                    const f1 = (420.0 / s) * kTwoPi;
                    const f2 = (1180.0 / s) * kTwoPi;
                    const f3 = (2450.0 / s) * kTwoPi;
                    const decayTime = 0.045 * s;

                    for (let i = 1; i < this.kKernelSize; ++i) {
                        const t = i * dt;
                        const env = Math.exp(-t / decayTime);
                        const mode1 = Math.sin(f1 * t) * 0.45;
                        const mode2 = Math.sin(f2 * t) * 0.35;
                        const mode3 = Math.sin(f3 * t) * 0.15;
                        this.kernel[i] = (mode1 + mode2 + mode3) * env;
                    }
                    break;
                }

                case 2: { // Water Coupled
                    this.kernel[0] = 0.80;
                    const f1 = (290.0 / s) * kTwoPi;
                    const f2 = (560.0 / s) * kTwoPi;
                    const decayTime = 0.030 * s;

                    for (let i = 1; i < this.kKernelSize; ++i) {
                        const t = i * dt;
                        const env = Math.exp(-t / decayTime);
                        const mode1 = Math.sin(f1 * t) * 0.60;
                        const mode2 = Math.sin(f2 * t) * 0.25;
                        this.kernel[i] = (mode1 + mode2) * env;
                    }
                    break;
                }

                case 3: { // Tiled Enclosure
                    this.kernel[0] = 0.60;
                    const flutterInterval = 0.0035 * s;
                    const decayTime = 0.060 * s;

                    for (let i = 1; i < this.kKernelSize; ++i) {
                        const t = i * dt;
                        const env = Math.exp(-t / decayTime);
                        const flutterPhase = (t % flutterInterval) / flutterInterval;
                        const flutterComb = (flutterPhase < 0.15 ? 0.35 : -0.05);
                        const wallMode = Math.sin((850.0 / s) * kTwoPi * t) * 0.25;
                        this.kernel[i] = (flutterComb + wallMode) * env;
                    }
                    break;
                }
            }

            // Peak normalization
            let maxAbs = 0.0;
            for (let i = 0; i < this.kKernelSize; ++i) {
                maxAbs = Math.max(maxAbs, Math.abs(this.kernel[i]));
            }
            if (maxAbs > 1.0e-5) {
                const norm = 0.85 / maxAbs;
                for (let i = 0; i < this.kKernelSize; ++i) {
                    this.kernel[i] = flushDenormal(this.kernel[i] * norm);
                }
            }
        }

        processSample(input, mix, size, model) {
            const safeMix = clamp(mix, 0.0, 1.0);
            if (safeMix <= 0.0001) return input;

            const targetModel = clamp(Math.round(model), 0, 3);
            const targetSize = clamp(size, 0.5, 2.0);
            if (targetModel !== this.currentModel || Math.abs(targetSize - this.currentSize) > 0.01) {
                this.rebuildKernel(targetModel, targetSize);
            }

            this.historyBuffer[this.writeIdx] = flushDenormal(input);

            let convSum = 0.0;
            let histIdx = this.writeIdx;

            for (let k = 0; k < this.kKernelSize; ++k) {
                convSum += this.kernel[k] * this.historyBuffer[histIdx];
                histIdx = (histIdx === 0) ? this.kKernelMask : (histIdx - 1);
            }

            this.writeIdx = (this.writeIdx + 1) & this.kKernelMask;
            convSum = flushDenormal(convSum);

            return flushDenormal((1.0 - safeMix) * input + safeMix * convSum);
        }
    }

    // =========================================================================
    // BoundedSaturator: C^1 Hermite Soft-Knee Saturator & Brickwall Limiter
    // =========================================================================
    class BoundedSaturator {
        static saturateHermite(x, knee = 0.72, ceiling = 1.05) {
            if (!Number.isFinite(x)) return 0.0;
            const absX = Math.abs(x);
            if (absX <= knee) return x;

            const sgn = x >= 0.0 ? 1.0 : -1.0;
            if (absX >= ceiling) return sgn * ceiling;

            const u = (absX - knee) / (ceiling - knee);
            const poly = u + u * u - u * u * u;
            return sgn * (knee + (ceiling - knee) * poly);
        }

        static processDrive(input, drive) {
            const d = clamp(drive, 0.0, 1.0);
            const driven = input * (1.0 + 3.5 * d);
            return BoundedSaturator.saturateHermite(driven, 0.72, 1.05);
        }

        static limitSample(input, masterGainLinear = 1.0) {
            if (!Number.isFinite(input)) return 0.0;
            const x = flushDenormal(input * masterGainLinear);
            const kKnee = 0.85;
            const kCeiling = 1.00;

            const absX = Math.abs(x);
            if (absX <= kKnee) return x;

            const sgn = x >= 0.0 ? 1.0 : -1.0;
            if (absX >= kCeiling) return sgn * kCeiling;

            const u = (absX - kKnee) / (kCeiling - kKnee);
            const poly = u * (1.0 + u * (1.0 - u));
            return clamp(sgn * (kKnee + (kCeiling - kKnee) * poly), -1.0, 1.0);
        }
    }

    // =========================================================================
    // VoiceManager: Mono-Legato, Poly 8-Voice, Stereo Unison
    // =========================================================================
    class VoiceManager {
        constructor() {
            this.kMaxPolyVoices = 8;
            this.sampleRate = 48000.0;
            this.voices = [];
            for (let i = 0; i < this.kMaxPolyVoices; ++i) {
                this.voices.push(new PhysicalVoice());
            }

            this.noteStack = [];
            this.lastVoiceMode = -1;
            this.damperPedalDown = false;
            this.pitchBendSemi = 0.0;
            this.channelPressure = 0.0;
            this.lastMonoNote = -1;
        }

        prepare(sampleRate) {
            this.sampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
            for (const v of this.voices) {
                v.prepare(sampleRate);
            }
            this.reset();
        }

        reset() {
            for (const v of this.voices) {
                v.reset();
            }
            this.noteStack = [];
            this.lastVoiceMode = -1;
            this.damperPedalDown = false;
            this.pitchBendSemi = 0.0;
            this.channelPressure = 0.0;
            this.lastMonoNote = -1;
        }

        findVoiceToSteal() {
            // Tier 1: Inactive voices
            for (let i = 0; i < this.kMaxPolyVoices; ++i) {
                if (!this.voices[i].isActive()) return i;
            }

            // Tier 2: Released voices (oldest first)
            let bestCandidate = -1;
            let oldestAge = 0;
            for (let i = 0; i < this.kMaxPolyVoices; ++i) {
                if (this.voices[i].isReleased() && !this.voices[i].pedalLatched) {
                    if (this.voices[i].ageSamples > oldestAge) {
                        oldestAge = this.voices[i].ageSamples;
                        bestCandidate = i;
                    }
                }
            }
            if (bestCandidate >= 0) return bestCandidate;

            // Tier 3: Oldest held voice
            oldestAge = 0;
            for (let i = 0; i < this.kMaxPolyVoices; ++i) {
                if (this.voices[i].ageSamples > oldestAge) {
                    oldestAge = this.voices[i].ageSamples;
                    bestCandidate = i;
                }
            }
            return bestCandidate >= 0 ? bestCandidate : 0;
        }

        handleNoteOn(noteNumber, velocity, params) {
            if (params.voice_mode !== this.lastVoiceMode) {
                this.noteStack = [];
                this.lastVoiceMode = params.voice_mode;
            }

            const sIdx = params.sample_index || 0;
            const sStart = params.sample_start || 0.0;
            const sRev = !!params.sample_reverse;
            const cData = params.customAudioData || null;

            if (params.voice_mode === 0) { // Mono Legato Mode
                this.noteStack.push({ noteNumber, velocity });
                const targetFreq = noteToFreq(noteNumber + this.pitchBendSemi);

                if (!this.voices[0].isActive()) {
                    this.voices[0].noteOn(noteNumber, velocity, 0.0, sIdx, sStart, sRev, cData);
                    this.lastMonoNote = noteNumber;
                } else {
                    this.voices[0].setFrequencyTarget(targetFreq, params.glide_time);
                    this.lastMonoNote = noteNumber;
                }
            }
            else if (params.voice_mode === 2) { // Stereo Unison Detune Mode
                const detuneCents = [-7.0, 7.0, -14.0, 14.0];
                const phaseOffsets = [0.000, 0.008, 0.016, 0.024];
                for (let i = 0; i < 4; ++i) {
                    const semiOffset = detuneCents[i] * 0.01 + this.pitchBendSemi;
                    const freq = noteToFreq(noteNumber + semiOffset);
                    const vStart = clamp(sStart + phaseOffsets[i], 0.0, 0.95);
                    this.voices[i].noteOn(noteNumber, velocity, freq, sIdx, vStart, sRev, cData);
                    this.voices[i].setFrequencyTarget(freq, params.glide_time);
                }
            }
            else { // Polyphonic 8-Voice Mode
                let voiceIdx = -1;
                for (let i = 0; i < this.kMaxPolyVoices; ++i) {
                    if (this.voices[i].isActive() && this.voices[i].noteNumber === noteNumber) {
                        voiceIdx = i;
                        break;
                    }
                }

                if (voiceIdx < 0) {
                    voiceIdx = this.findVoiceToSteal();
                }

                this.voices[voiceIdx].noteOn(noteNumber, velocity, 0.0, sIdx, sStart, sRev, cData);
                this.voices[voiceIdx].setPitchBend(this.pitchBendSemi);
                this.voices[voiceIdx].setAftertouch(this.channelPressure);
            }
        }

        handleNoteOff(noteNumber, params) {
            if (params.voice_mode !== this.lastVoiceMode) {
                this.noteStack = [];
                this.lastVoiceMode = params.voice_mode;
            }

            if (this.damperPedalDown) {
                for (const v of this.voices) {
                    if (v.isActive() && v.noteNumber === noteNumber) {
                        v.pedalLatched = true;
                    }
                }
            }

            if (params.voice_mode === 0) { // Mono Legato Mode
                this.noteStack = this.noteStack.filter(item => item.noteNumber !== noteNumber);
                if (this.noteStack.length > 0) {
                    const top = this.noteStack[this.noteStack.length - 1];
                    const topFreq = noteToFreq(top.noteNumber + this.pitchBendSemi);
                    this.voices[0].setFrequencyTarget(topFreq, params.glide_time > 0.0 ? params.glide_time : 50.0);
                    this.lastMonoNote = top.noteNumber;
                    return;
                }

                this.voices[0].noteOff();
                this.lastMonoNote = -1;
                return;
            }

            // Polyphonic or Stereo Unison
            for (const v of this.voices) {
                if (v.isActive() && v.noteNumber === noteNumber) {
                    v.noteOff();
                }
            }
        }

        allNotesOff() {
            this.noteStack = [];
            for (const v of this.voices) {
                v.noteOff();
            }
        }

        setPitchBend(semitones) {
            this.pitchBendSemi = semitones;
            for (const v of this.voices) {
                if (v.isActive()) {
                    v.setPitchBend(this.pitchBendSemi);
                }
            }
        }

        processSample(params) {
            let outL = 0.0, outR = 0.0;
            let outAirVelocity = 0.0, outAperture = 0.0;
            let outBubbleActivity = 0.0, outDropletPop = 0.0;

            if (params.voice_mode !== this.lastVoiceMode) {
                this.noteStack = [];
                this.lastVoiceMode = params.voice_mode;
            }

            if (params.voice_mode === 0) { // Mono Legato Mode
                const res = this.voices[0].processSample(params);
                outL = res.signal;
                outR = res.signal;
                outAirVelocity = res.airVelocity;
                outAperture = res.aperture;
                outBubbleActivity = res.bubbleActivity;
                outDropletPop = res.dropletPop;
            }
            else if (params.voice_mode === 2) { // Stereo Unison Detune Mode
                const panL = [0.85, 0.15, 0.65, 0.35];
                const panR = [0.15, 0.85, 0.35, 0.65];

                let maxAirVel = 0.0, maxAp = 0.0, sumBubbles = 0.0, maxPop = 0.0;

                for (let i = 0; i < 4; ++i) {
                    const res = this.voices[i].processSample(params);
                    outL += res.signal * panL[i];
                    outR += res.signal * panR[i];

                    maxAirVel = Math.max(maxAirVel, res.airVelocity);
                    maxAp = Math.max(maxAp, res.aperture);
                    sumBubbles += res.bubbleActivity;
                    maxPop = Math.max(maxPop, res.dropletPop);
                }

                outL *= 0.50;
                outR *= 0.50;
                outAirVelocity = maxAirVel;
                outAperture = maxAp;
                outBubbleActivity = clamp(sumBubbles * 0.25, 0.0, 1.0);
                outDropletPop = maxPop;
            }
            else { // Polyphonic 8-Voice Mode
                let sumL = 0.0, sumR = 0.0;
                let maxAirVel = 0.0, maxAp = 0.0, sumBubbles = 0.0, maxPop = 0.0;
                let activeCount = 0;

                for (let i = 0; i < this.kMaxPolyVoices; ++i) {
                    if (!this.voices[i].isActive()) continue;
                    activeCount++;

                    const res = this.voices[i].processSample(params);
                    const pan = 0.40 + 0.20 * (i % 3);
                    sumL += res.signal * pan;
                    sumR += res.signal * (1.0 - pan);

                    maxAirVel = Math.max(maxAirVel, res.airVelocity);
                    maxAp = Math.max(maxAp, res.aperture);
                    sumBubbles += res.bubbleActivity;
                    maxPop = Math.max(maxPop, res.dropletPop);
                }

                if (activeCount > 1) {
                    const norm = 1.0 / Math.sqrt(activeCount);
                    sumL *= norm;
                    sumR *= norm;
                }

                outL = sumL;
                outR = sumR;
                outAirVelocity = maxAirVel;
                outAperture = maxAp;
                outBubbleActivity = clamp(sumBubbles * 0.25, 0.0, 1.0);
                outDropletPop = maxPop;
            }

            return {
                outL: flushDenormal(outL),
                outR: flushDenormal(outR),
                airVelocity: outAirVelocity,
                aperture: outAperture,
                bubbleActivity: outBubbleActivity,
                dropletPop: outDropletPop
            };
        }
    }

    // =========================================================================
    // Macro Coupling Helper
    // =========================================================================
    function applyMacroCoupling(inParams) {
        const out = { ...inParams };
        const ms = clamp(inParams.macro_squeeze !== undefined ? inParams.macro_squeeze : 0.5, 0.0, 1.0);
        const mm = clamp(inParams.macro_moisture !== undefined ? inParams.macro_moisture : 0.2, 0.0, 1.0);

        const rawPressure = inParams.param_pressure !== undefined ? inParams.param_pressure : (inParams.pressure !== undefined ? inParams.pressure : 0.70);
        const rawTension = inParams.param_tension !== undefined ? inParams.param_tension : (inParams.tension !== undefined ? inParams.tension : 0.50);
        const rawFlutter = inParams.param_flutter !== undefined ? inParams.param_flutter : (inParams.flutter !== undefined ? inParams.flutter : 0.20);

        const rawVisc = inParams.param_viscosity !== undefined ? inParams.param_viscosity : (inParams.viscosity !== undefined ? inParams.viscosity : 0.25);
        const rawMoist = inParams.param_moisture !== undefined ? inParams.param_moisture : (inParams.moisture !== undefined ? inParams.moisture : 0.20);
        const rawDrop = inParams.param_droplet_rate !== undefined ? inParams.param_droplet_rate : (inParams.droplet_rate !== undefined ? inParams.droplet_rate : 0.30);

        const effPressure = clamp(rawPressure * (0.5 + 0.8 * ms), 0.0, 1.0);
        const effTension = clamp(rawTension * (0.7 + 0.6 * ms), 0.0, 1.0);
        const effFlutter = clamp(rawFlutter + 0.4 * (ms * ms), 0.0, 1.0);

        const effVisc = clamp(rawVisc * (0.4 + 1.2 * mm), 0.0, 1.0);
        const effMoist = clamp(rawMoist * (0.2 + 1.6 * mm), 0.0, 1.0);
        const effDrop = clamp(rawDrop * Math.pow(mm, 1.5), 0.0, 1.0);

        out.param_pressure = effPressure;
        out.pressure = effPressure;
        out.param_tension = effTension;
        out.tension = effTension;
        out.param_flutter = effFlutter;
        out.flutter = effFlutter;

        out.param_viscosity = effVisc;
        out.viscosity = effVisc;
        out.param_moisture = effMoist;
        out.moisture = effMoist;
        out.param_droplet_rate = effDrop;
        out.droplet_rate = effDrop;

        return out;
    }

    // =========================================================================
    // Factory Presets Database
    // =========================================================================
    const FACTORY_PRESETS = [
        {
            index: 0,
            name: "01 - Clean Continental Purr",
            sample_index: 3,
            params: {
                param_pressure: 0.65, param_tension: 0.42, param_aperture: 0.38, param_flutter: 0.35,
                param_viscosity: 0.50, param_moisture: 0.25, param_droplet_rate: 0.40,
                param_cleft_damping: 0.55, param_porcelain_mix: 0.20, param_porcelain_size: 1.00, param_porcelain_model: 1,
                param_voice_mode: 0, param_glide_time: 35.0, param_sub_level: -3.0, param_drive: 0.15, param_master_gain: 0.00,
                macro_squeeze: 0.45, macro_moisture: 0.25,
                param_env_attack: 8.0, param_env_decay: 650.0, param_env_sustain: 0.40, param_env_release: 180.0
            }
        },
        {
            index: 1,
            name: "02 - High-Tension Squeaker",
            sample_index: 6,
            params: {
                param_pressure: 0.85, param_tension: 0.92, param_aperture: 0.08, param_flutter: 0.18,
                param_viscosity: 0.12, param_moisture: 0.15, param_droplet_rate: 0.10,
                param_cleft_damping: 0.35, param_porcelain_mix: 0.18, param_porcelain_size: 0.75, param_porcelain_model: 0,
                param_voice_mode: 0, param_glide_time: 25.0, param_sub_level: -20.0, param_drive: 0.28, param_master_gain: -1.00,
                macro_squeeze: 0.85, macro_moisture: 0.15,
                param_env_attack: 2.0, param_env_decay: 220.0, param_env_sustain: 0.65, param_env_release: 55.0
            }
        },
        {
            index: 2,
            name: "03 - Viscous Multiphase Splatter",
            sample_index: 7,
            params: {
                param_pressure: 0.78, param_tension: 0.40, param_aperture: 0.45, param_flutter: 0.40,
                param_viscosity: 0.85, param_moisture: 0.80, param_droplet_rate: 0.82,
                param_cleft_damping: 0.42, param_porcelain_mix: 0.42, param_porcelain_size: 1.15, param_porcelain_model: 2,
                param_voice_mode: 0, param_glide_time: 50.0, param_sub_level: -4.0, param_drive: 0.28, param_master_gain: 0.00,
                macro_squeeze: 0.60, macro_moisture: 0.85,
                param_env_attack: 5.0, param_env_decay: 550.0, param_env_sustain: 0.42, param_env_release: 160.0
            }
        },
        {
            index: 3,
            name: "04 - Visceral Sub-Rumble (18 Hz)",
            sample_index: 1,
            params: {
                param_pressure: 0.90, param_tension: 0.12, param_aperture: 0.58, param_flutter: 0.30,
                param_viscosity: 0.35, param_moisture: 0.28, param_droplet_rate: 0.22,
                param_cleft_damping: 0.75, param_porcelain_mix: 0.48, param_porcelain_size: 1.55, param_porcelain_model: 1,
                param_voice_mode: 0, param_glide_time: 75.0, param_sub_level: 3.0, param_drive: 0.42, param_master_gain: 0.50,
                macro_squeeze: 0.70, macro_moisture: 0.35,
                param_env_attack: 18.0, param_env_decay: 850.0, param_env_sustain: 0.75, param_env_release: 280.0
            }
        },
        {
            index: 4,
            name: "05 - Flutter-Tongue Stutter",
            sample_index: 5,
            params: {
                param_pressure: 0.75, param_tension: 0.48, param_aperture: 0.32, param_flutter: 0.95,
                param_viscosity: 0.28, param_moisture: 0.32, param_droplet_rate: 0.28,
                param_cleft_damping: 0.42, param_porcelain_mix: 0.28, param_porcelain_size: 1.05, param_porcelain_model: 0,
                param_voice_mode: 0, param_glide_time: 30.0, param_sub_level: -9.0, param_drive: 0.24, param_master_gain: 0.00,
                macro_squeeze: 0.80, macro_moisture: 0.35,
                param_env_attack: 5.0, param_env_decay: 480.0, param_env_sustain: 0.55, param_env_release: 90.0
            }
        },
        {
            index: 5,
            name: "06 - Wet Porcelain Slam",
            sample_index: 2,
            params: {
                param_pressure: 0.82, param_tension: 0.36, param_aperture: 0.42, param_flutter: 0.35,
                param_viscosity: 0.72, param_moisture: 0.78, param_droplet_rate: 0.65,
                param_cleft_damping: 0.32, param_porcelain_mix: 0.72, param_porcelain_size: 1.35, param_porcelain_model: 1,
                param_voice_mode: 0, param_glide_time: 45.0, param_sub_level: -2.0, param_drive: 0.32, param_master_gain: 0.00,
                macro_squeeze: 0.65, macro_moisture: 0.80,
                param_env_attack: 8.0, param_env_decay: 620.0, param_env_sustain: 0.48, param_env_release: 210.0
            }
        },
        {
            index: 6,
            name: "07 - Micro-Puff Staccato",
            sample_index: 0,
            params: {
                param_pressure: 0.62, param_tension: 0.62, param_aperture: 0.22, param_flutter: 0.12,
                param_viscosity: 0.18, param_moisture: 0.15, param_droplet_rate: 0.18,
                param_cleft_damping: 0.65, param_porcelain_mix: 0.12, param_porcelain_size: 0.85, param_porcelain_model: 0,
                param_voice_mode: 0, param_glide_time: 0.0, param_sub_level: -15.0, param_drive: 0.10, param_master_gain: 1.50,
                macro_squeeze: 0.35, macro_moisture: 0.18,
                param_env_attack: 0.5, param_env_decay: 70.0, param_env_sustain: 0.00, param_env_release: 20.0
            }
        },
        {
            index: 7,
            name: "08 - Extended Gaseous Drift",
            sample_index: 1,
            params: {
                param_pressure: 0.58, param_tension: 0.32, param_aperture: 0.52, param_flutter: 0.42,
                param_viscosity: 0.22, param_moisture: 0.18, param_droplet_rate: 0.12,
                param_cleft_damping: 0.48, param_porcelain_mix: 0.32, param_porcelain_size: 1.05, param_porcelain_model: 3,
                param_voice_mode: 0, param_glide_time: 110.0, param_sub_level: -6.0, param_drive: 0.18, param_master_gain: 0.00,
                macro_squeeze: 0.40, macro_moisture: 0.22,
                param_env_attack: 35.0, param_env_decay: 1750.0, param_env_sustain: 0.70, param_env_release: 380.0
            }
        },
        {
            index: 8,
            name: "09 - Unison Twin Cannons",
            sample_index: 3,
            params: {
                param_pressure: 0.85, param_tension: 0.40, param_aperture: 0.38, param_flutter: 0.52,
                param_viscosity: 0.35, param_moisture: 0.32, param_droplet_rate: 0.35,
                param_cleft_damping: 0.38, param_porcelain_mix: 0.48, param_porcelain_size: 1.20, param_porcelain_model: 1,
                param_voice_mode: 2, param_glide_time: 45.0, param_sub_level: 0.0, param_drive: 0.38, param_master_gain: -2.00,
                macro_squeeze: 0.75, macro_moisture: 0.35,
                param_env_attack: 10.0, param_env_decay: 520.0, param_env_sustain: 0.68, param_env_release: 160.0
            }
        },
        {
            index: 9,
            name: "10 - The Brown Note 808",
            sample_index: 3,
            params: {
                param_pressure: 0.95, param_tension: 0.10, param_aperture: 0.28, param_flutter: 0.18,
                param_viscosity: 0.25, param_moisture: 0.18, param_droplet_rate: 0.15,
                param_cleft_damping: 0.88, param_porcelain_mix: 0.25, param_porcelain_size: 1.55, param_porcelain_model: 1,
                param_voice_mode: 0, param_glide_time: 15.0, param_sub_level: 6.0, param_drive: 0.52, param_master_gain: 1.00,
                macro_squeeze: 0.90, macro_moisture: 0.20,
                param_env_attack: 1.0, param_env_decay: 1150.0, param_env_sustain: 0.35, param_env_release: 260.0
            }
        }
    ];

    // =========================================================================
    // WebAudioEngine: Complete Web Audio Client Synthesis System
    // =========================================================================
    class WebAudioEngine {
        constructor() {
            this.ctx = null;
            this.processorNode = null;
            this.isInitialized = false;
            this.sampleRate = 48000.0;
            this.onTelemetry = null; // Callback: (frame) => void

            // Core DSP Modules
            this.voiceManager = new VoiceManager();
            this.waveguideL = new CleftWaveguide();
            this.waveguideR = new CleftWaveguide();
            this.convolverL = new PorcelainConvolver();
            this.convolverR = new PorcelainConvolver();
            this.dcBlockerL = new SubBassDcBlocker();
            this.dcBlockerR = new SubBassDcBlocker();

            // Active Parameters
            this.currentPreset = 0;
            this.params = { ...FACTORY_PRESETS[0].params };

            // Telemetry Accumulation
            this.telemetrySampleCounter = 0;
            this.telemetryIntervalSamples = 800; // ~60 Hz at 48 kHz
            this.scopeWriteIdx = 0;
            this.scopeSamples = new Float32Array(kTelemetryScopeSamples);
            this.rmsAccumL = 0.0;
            this.rmsAccumR = 0.0;
            this.currentAirVelocity = 0.0;
            this.currentAperture = 0.35;
            this.currentPressure = 0.70;
            this.currentBubbleDensity = 0.20;
            this.currentDropletPops = 0.0;
            this.currentCleftEnergy = 0.0;

            // Preallocate uint8 byte buffer for zero-alloc base64 telemetry
            this.scopeBytes = new Uint8Array(kTelemetryScopeSamples);

            // Sample Bank & Pitcher State
            this.selectedSampleIndex = 3; // Default to Slot 4: Iconic Meme (4gcs5k8n-FY)
            this.sampleStartOffset = 0.0;
            this.sampleReverse = false;
            this.customAudioData = null;
            this.customAudioName = null;

            this._initAutoplayListeners();
        }

        _initAutoplayListeners() {
            if (typeof window === 'undefined' || typeof window.addEventListener !== 'function') return;
            const resumeHandler = () => {
                this.ensureAudioContext();
            };

            window.addEventListener('pointerdown', resumeHandler, { passive: true });
            window.addEventListener('keydown', resumeHandler, { passive: true });
            window.addEventListener('touchstart', resumeHandler, { passive: true });
            window.addEventListener('click', resumeHandler, { passive: true });
        }

        ensureAudioContext() {
            if (!this.ctx) {
                const AudioCtxClass = (typeof window !== 'undefined' && (window.AudioContext || window.webkitAudioContext)) ||
                                      (typeof globalThis !== 'undefined' && (globalThis.AudioContext || globalThis.webkitAudioContext));
                if (!AudioCtxClass) {
                    return false;
                }
                this.ctx = new AudioCtxClass();
                this.sampleRate = this.ctx.sampleRate || 48000.0;
                this._initDspGraph();
            }

            if (this.ctx.state === 'suspended') {
                this.ctx.resume().then(() => {
                    this._updateHeaderStatus('AUDIO RUNNING');
                }).catch(err => {
                    console.warn('[WebAudioEngine] AudioContext resume failed:', err);
                });
            } else if (this.ctx.state === 'running') {
                this._updateHeaderStatus('AUDIO RUNNING');
            }

            return true;
        }

        _updateHeaderStatus(text) {
            if (typeof document === 'undefined' || typeof document.querySelector !== 'function') return;
            const badgeSpan = document.querySelector('.header-status .status-badge span');
            if (badgeSpan) {
                badgeSpan.textContent = text;
            }
        }

        _initDspGraph() {
            if (this.isInitialized) return;

            this.voiceManager.prepare(this.sampleRate);
            this.waveguideL.prepare(this.sampleRate);
            this.waveguideR.prepare(this.sampleRate);
            this.convolverL.prepare(this.sampleRate);
            this.convolverR.prepare(this.sampleRate);
            this.dcBlockerL.prepare(this.sampleRate);
            this.dcBlockerR.prepare(this.sampleRate);

            this.telemetryIntervalSamples = Math.max(64, Math.floor(this.sampleRate / 60.0));

            // Use 512-sample ScriptProcessorNode for universal, zero-CORS compatibility across file://, http, and webviews
            const bufferSize = 512;
            this.processorNode = this.ctx.createScriptProcessor(bufferSize, 0, 2);
            this.processorNode.onaudioprocess = (e) => this._processAudio(e);

            // Connect to audio destination
            this.processorNode.connect(this.ctx.destination);
            this.isInitialized = true;
            console.log(`[WebAudioEngine] Engine initialized. SampleRate: ${this.sampleRate} Hz, Buffer: ${bufferSize}`);
        }

        _processAudio(e) {
            const outputBuffer = e.outputBuffer;
            const leftChannel = outputBuffer.getChannelData(0);
            const rightChannel = outputBuffer.getChannelData(1);
            const numSamples = leftChannel.length;

            const effParams = applyMacroCoupling(this.params);
            const masterGainLinear = dbToGain(effParams.param_master_gain);

            const decimationFactor = Math.max(1, Math.floor(this.telemetryIntervalSamples / kTelemetryScopeSamples));

            for (let i = 0; i < numSamples; ++i) {
                // 1. Voice Manager
                const vRes = this.voiceManager.processSample({
                    pressure: effParams.param_pressure,
                    tension: effParams.param_tension,
                    aperture: effParams.param_aperture,
                    flutter: effParams.param_flutter,
                    viscosity: effParams.param_viscosity,
                    moisture: effParams.param_moisture,
                    droplet_rate: effParams.param_droplet_rate,
                    sub_level: effParams.param_sub_level,
                    voice_mode: Math.round(effParams.param_voice_mode),
                    glide_time: effParams.param_glide_time,
                    env_attack: effParams.param_env_attack,
                    env_decay: effParams.param_env_decay,
                    env_sustain: effParams.param_env_sustain,
                    env_release: effParams.param_env_release,
                    sample_index: this.selectedSampleIndex,
                    sample_start: this.sampleStartOffset,
                    sample_reverse: this.sampleReverse,
                    customAudioData: this.customAudioData
                });

                // 2. Intergluteal Cleft Waveguide
                const wgL = this.waveguideL.processSample(vRes.outL, effParams.param_cleft_damping);
                const wgR = this.waveguideR.processSample(vRes.outR, effParams.param_cleft_damping);

                // 3. Ceramic Porcelain Cavity Convolver
                const convL = this.convolverL.processSample(wgL, effParams.param_porcelain_mix, effParams.param_porcelain_size, effParams.param_porcelain_model);
                const convR = this.convolverR.processSample(wgR, effParams.param_porcelain_mix, effParams.param_porcelain_size, effParams.param_porcelain_model);

                // 4. Infrasonic 15 Hz DC Blocker
                const dcL = this.dcBlockerL.processSample(convL);
                const dcR = this.dcBlockerR.processSample(convR);

                // 5. Harmonic Saturation Drive Stage
                const satL = BoundedSaturator.processDrive(dcL, effParams.param_drive);
                const satR = BoundedSaturator.processDrive(dcR, effParams.param_drive);

                // 6. Master True-Peak Brickwall Limiter
                const outL = BoundedSaturator.limitSample(satL, masterGainLinear);
                const outR = BoundedSaturator.limitSample(satR, masterGainLinear);

                leftChannel[i] = outL;
                rightChannel[i] = outR;

                // 7. Telemetry Decimation & Accumulation
                if (this.scopeWriteIdx < kTelemetryScopeSamples) {
                    if ((i % decimationFactor) === 0) {
                        this.scopeSamples[this.scopeWriteIdx++] = (outL + outR) * 0.5;
                    }
                }

                this.rmsAccumL += outL * outL;
                this.rmsAccumR += outR * outR;

                this.currentAperture = vRes.aperture;
                this.currentAirVelocity = vRes.airVelocity;
                this.currentPressure = effParams.param_pressure;
                this.currentBubbleDensity = vRes.bubbleActivity;
                if (vRes.dropletPop > this.currentDropletPops) {
                    this.currentDropletPops = vRes.dropletPop;
                }
                this.currentCleftEnergy = (this.waveguideL.getResonanceEnergy() + this.waveguideR.getResonanceEnergy()) * 0.5;

                if (++this.telemetrySampleCounter >= this.telemetryIntervalSamples) {
                    this.telemetrySampleCounter = 0;
                    const invCount = 1.0 / this.telemetryIntervalSamples;
                    const rmsL = Math.sqrt(flushDenormal(this.rmsAccumL * invCount));
                    const rmsR = Math.sqrt(flushDenormal(this.rmsAccumR * invCount));

                    // Base64 encode the 128 scope samples
                    for (let sIdx = 0; sIdx < kTelemetryScopeSamples; ++sIdx) {
                        const s = clamp(this.scopeSamples[sIdx], -1.0, 1.0);
                        this.scopeBytes[sIdx] = Math.round(128.0 + s * 127.0);
                    }

                    let binary = '';
                    for (let b = 0; b < kTelemetryScopeSamples; ++b) {
                        binary += String.fromCharCode(this.scopeBytes[b]);
                    }
                    const b64 = btoa(binary);

                    const frame = {
                        waveform: b64,
                        pressure: this.currentPressure,
                        velocity: this.currentAirVelocity,
                        aperture: this.currentAperture,
                        bubbleDensity: this.currentBubbleDensity,
                        dropletPops: this.currentDropletPops,
                        cleftEnergy: this.currentCleftEnergy,
                        rmsL: rmsL,
                        rmsR: rmsR
                    };

                    if (typeof this.onTelemetry === 'function') {
                        this.onTelemetry(frame);
                    }

                    // Reset accumulators
                    this.scopeWriteIdx = 0;
                    this.rmsAccumL = 0.0;
                    this.rmsAccumR = 0.0;
                    this.currentDropletPops = 0.0;
                }
            }
        }

        // Public Control Interface
        noteOn(noteNumber, velocity = 0.8) {
            this.ensureAudioContext();
            const effParams = applyMacroCoupling(this.params);
            this.voiceManager.handleNoteOn(noteNumber, velocity, {
                voice_mode: Math.round(effParams.param_voice_mode),
                glide_time: effParams.param_glide_time,
                sample_index: this.selectedSampleIndex,
                sample_start: this.sampleStartOffset,
                sample_reverse: this.sampleReverse,
                customAudioData: this.customAudioData
            });
        }

        noteOff(noteNumber, velocity = 0.0) {
            const effParams = applyMacroCoupling(this.params);
            this.voiceManager.handleNoteOff(noteNumber, {
                voice_mode: Math.round(effParams.param_voice_mode),
                glide_time: effParams.param_glide_time,
                sample_index: this.selectedSampleIndex,
                sample_start: this.sampleStartOffset,
                sample_reverse: this.sampleReverse,
                customAudioData: this.customAudioData
            });
        }

        allNotesOff() {
            this.voiceManager.allNotesOff();
        }

        pitchBend(cents) {
            const semitones = Number(cents) / 100.0;
            this.voiceManager.setPitchBend(semitones);
        }

        setSampleIndex(index) {
            this.selectedSampleIndex = clamp(Math.round(index), 0, 7);
            this.customAudioData = null; // Switch to embedded sample
        }

        setSampleStart(offset) {
            this.sampleStartOffset = clamp(Number(offset), 0.0, 1.0);
        }

        setSampleReverse(reverse) {
            this.sampleReverse = !!reverse;
        }

        async loadCustomAudio(arrayBuffer, fileName) {
            this.ensureAudioContext();
            try {
                const audioBuffer = await this.ctx.decodeAudioData(arrayBuffer);
                const channelData = audioBuffer.getChannelData(0);
                this.customAudioData = new Float32Array(channelData);
                this.customAudioName = fileName || "Custom Audio";
                console.log(`[WebAudioEngine] Loaded custom audio "${this.customAudioName}": ${channelData.length} samples (${(channelData.length / audioBuffer.sampleRate).toFixed(2)} s)`);
                return {
                    name: this.customAudioName,
                    samples: channelData.length,
                    duration: channelData.length / audioBuffer.sampleRate
                };
            } catch (err) {
                console.error('[WebAudioEngine] Failed to decode custom audio:', err);
                return null;
            }
        }

        setParameter(paramId, value) {
            this.ensureAudioContext();
            const numVal = Number(value);
            this.params[paramId] = numVal;
            if (paramId === 'sample_index' || paramId === 'param_sample_index') {
                this.setSampleIndex(numVal);
            } else if (paramId === 'sample_start' || paramId === 'param_sample_start') {
                this.setSampleStart(numVal);
            } else if (paramId === 'sample_reverse' || paramId === 'param_sample_reverse') {
                this.setSampleReverse(!!numVal);
            }
        }

        loadPreset(presetIndex) {
            const idx = clamp(Math.round(presetIndex), 0, FACTORY_PRESETS.length - 1);
            this.currentPreset = idx;
            const preset = FACTORY_PRESETS[idx];
            this.params = { ...preset.params };
            if (typeof preset.sample_index !== 'undefined') {
                this.selectedSampleIndex = preset.sample_index;
            }
            return {
                index: idx,
                name: preset.name,
                params: this.getParams()
            };
        }

        getCurrentPreset() {
            return this.currentPreset;
        }

        getPresetName(index) {
            const idx = clamp(Math.round(index), 0, FACTORY_PRESETS.length - 1);
            return FACTORY_PRESETS[idx].name;
        }

        getParams() {
            return {
                ...this.params,
                sample_index: this.selectedSampleIndex,
                param_sample_index: this.selectedSampleIndex,
                sample_start: this.sampleStartOffset,
                param_sample_start: this.sampleStartOffset,
                sample_reverse: this.sampleReverse ? 1 : 0,
                param_sample_reverse: this.sampleReverse ? 1 : 0
            };
        }
    }

    // Export to global scope
    global.WebAudioEngine = WebAudioEngine;
    global.PPF42_PRESETS = FACTORY_PRESETS;

})(typeof window !== 'undefined' ? window : globalThis);
