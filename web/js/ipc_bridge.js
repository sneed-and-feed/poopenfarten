/**
 * PPF-42 DYNAMICS - Bi-directional C++/JavaScript IPC Bridge
 * Interfaces with JUCE 8 WebBrowserComponent backend (window.__JUCE__.backend)
 * Provides automatic fallback/simulation when running in a standalone browser.
 */

class IPCBridge {
    constructor() {
        this.isJuce = !!(window.__IS_JUCE__ || (window.__JUCE__ && window.__JUCE__.backend));
        this.listeners = new Map();
        this.simTimer = null;
        this.simPhase = 0;

        this._initBackend();
    }

    _initBackend() {
        if (window.__JUCE__ && window.__JUCE__.backend) {
            this.backend = window.__JUCE__.backend;
            console.log('[IPCBridge] Connected to native JUCE 8 backend.');
        } else {
            console.log('[IPCBridge] Standalone browser mode: active simulation enabled.');
            this.backend = null;
            this._startSimulation();
        }
    }

    addEventListener(eventName, callback) {
        if (!this.listeners.has(eventName)) {
            this.listeners.set(eventName, []);
        }
        this.listeners.get(eventName).push(callback);

        if (this.backend && typeof this.backend.addEventListener === 'function') {
            this.backend.addEventListener(eventName, callback);
        }
    }

    emit(eventName, payload) {
        if (this.backend && typeof this.backend.emitEvent === 'function') {
            try {
                this.backend.emitEvent(eventName, payload);
            } catch (err) {
                console.warn('[IPCBridge] Backend emitEvent failed:', err);
            }
        }
    }

    setParameter(paramId, value) {
        this.emit('paramChange', { id: paramId, value: Number(value) });
    }

    loadPreset(presetIndex) {
        this.emit('loadPreset', { index: Number(presetIndex) });
    }

    noteOn(noteNumber, velocity = 0.8) {
        this.emit('noteOn', { note: Math.round(noteNumber), velocity: Number(velocity) });
    }

    noteOff(noteNumber, velocity = 0.0) {
        this.emit('noteOff', { note: Math.round(noteNumber), velocity: Number(velocity) });
    }

    allNotesOff() {
        this.emit('allNotesOff', {});
    }

    pitchBend(cents) {
        this.emit('pitchBend', { cents: Number(cents) });
    }

    requestState() {
        this.emit('requestState', {});
    }

    _notifyLocal(eventName, data) {
        const list = this.listeners.get(eventName);
        if (list) {
            for (const cb of list) {
                try {
                    cb(data);
                } catch (e) {
                    console.error('[IPCBridge] Callback error:', e);
                }
            }
        }
    }

    _startSimulation() {
        // Generates realistic simulated relaxation oscillation frames for standalone browser testing
        this.simTimer = setInterval(() => {
            this.simPhase += 0.05;
            const f0 = 45.0; // Hz
            const N = 128;
            const bytes = new Uint8Array(N);

            for (let i = 0; i < N; ++i) {
                const t = this.simPhase + (i / N) * 2.0;
                // Asymmetric relaxation oscillation curve (sawtooth-like valve reed with Bernoulli collapse)
                const saw = 2.0 * (t - Math.floor(t + 0.5));
                const reed = Math.sin(t * 6.28318) * 0.7 + (saw > 0 ? 0.3 : -0.2);
                const clamped = Math.max(-1.0, Math.min(1.0, reed));
                bytes[i] = Math.round(128 + clamped * 127);
            }

            let binary = '';
            for (let i = 0; i < N; ++i) {
                binary += String.fromCharCode(bytes[i]);
            }
            const b64 = btoa(binary);

            const frame = {
                waveform: b64,
                pressure: 0.65 + 0.1 * Math.sin(this.simPhase * 0.3),
                velocity: 18.0 + 8.0 * Math.sin(this.simPhase * 0.8),
                aperture: 0.35 + 0.15 * Math.sin(this.simPhase),
                bubbleDensity: 0.25 + 0.15 * Math.sin(this.simPhase * 1.2),
                dropletPops: Math.random() < 0.15 ? 1.0 : 0.0,
                cleftEnergy: 0.45,
                rmsL: 0.35 + 0.1 * Math.sin(this.simPhase),
                rmsR: 0.33 + 0.1 * Math.sin(this.simPhase)
            };

            this._notifyLocal('visualizerFrame', frame);
        }, 1000 / 60);
    }
}

window.ipcBridge = new IPCBridge();
