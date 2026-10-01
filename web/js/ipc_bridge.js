/**
 * PPF-42 DYNAMICS - Bi-directional C++/JavaScript IPC Bridge
 * Interfaces with JUCE 8 WebBrowserComponent backend (window.__JUCE__.backend)
 * Provides seamless routing to WebAudioEngine when running standalone in browsers.
 */

class IPCBridge {
    constructor() {
        this.isJuce = !!(window.__IS_JUCE__ || (window.__JUCE__ && window.__JUCE__.backend));
        this.listeners = new Map();
        this.audioEngine = null;
        this.simTimer = null;
        this.simPhase = 0;

        this._initBackend();
    }

    _initBackend() {
        if (window.__JUCE__ && window.__JUCE__.backend) {
            this.backend = window.__JUCE__.backend;
            console.log('[IPCBridge] Connected to native JUCE 8 backend.');
        } else {
            console.log('[IPCBridge] Standalone browser mode: WebAudio synthesis engine active.');
            this.backend = null;
            this._initWebAudioEngine();
        }
    }

    _initWebAudioEngine() {
        if (typeof WebAudioEngine !== 'undefined') {
            this.audioEngine = new WebAudioEngine();
            this.audioEngine.onTelemetry = (frame) => {
                this._notifyLocal('visualizerFrame', frame);
            };
            console.log('[IPCBridge] WebAudioEngine connected to telemetry visualizers.');
        } else {
            console.warn('[IPCBridge] WebAudioEngine not detected; running procedural simulation.');
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
        if (this.backend && typeof this.backend.emitEvent === 'function') {
            this.emit('paramChange', { id: paramId, value: Number(value) });
        } else if (this.audioEngine) {
            this.audioEngine.setParameter(paramId, Number(value));
        }
    }

    loadPreset(presetIndex) {
        if (this.backend && typeof this.backend.emitEvent === 'function') {
            this.emit('loadPreset', { index: Number(presetIndex) });
        } else if (this.audioEngine) {
            const presetData = this.audioEngine.loadPreset(Number(presetIndex));
            this._notifyLocal('stateSync', {
                currentPreset: presetData.index,
                presetName: presetData.name,
                params: presetData.params
            });
        }
    }

    noteOn(noteNumber, velocity = 0.8) {
        if (this.backend && typeof this.backend.emitEvent === 'function') {
            this.emit('noteOn', { note: Math.round(noteNumber), velocity: Number(velocity) });
        } else if (this.audioEngine) {
            this.audioEngine.noteOn(Math.round(noteNumber), Number(velocity));
        }
    }

    noteOff(noteNumber, velocity = 0.0) {
        if (this.backend && typeof this.backend.emitEvent === 'function') {
            this.emit('noteOff', { note: Math.round(noteNumber), velocity: Number(velocity) });
        } else if (this.audioEngine) {
            this.audioEngine.noteOff(Math.round(noteNumber), Number(velocity));
        }
    }

    allNotesOff() {
        if (this.backend && typeof this.backend.emitEvent === 'function') {
            this.emit('allNotesOff', {});
        } else if (this.audioEngine) {
            this.audioEngine.allNotesOff();
        }
    }

    pitchBend(cents) {
        if (this.backend && typeof this.backend.emitEvent === 'function') {
            this.emit('pitchBend', { cents: Number(cents) });
        } else if (this.audioEngine) {
            this.audioEngine.pitchBend(Number(cents));
        }
    }

    requestState() {
        if (this.backend && typeof this.backend.emitEvent === 'function') {
            this.emit('requestState', {});
        } else if (this.audioEngine) {
            const currentPreset = this.audioEngine.getCurrentPreset();
            this._notifyLocal('stateSync', {
                currentPreset: currentPreset,
                presetName: this.audioEngine.getPresetName(currentPreset),
                params: this.audioEngine.getParams()
            });
        }
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
        // Fallback procedural relaxation oscillation generator if Web Audio unavailable
        this.simTimer = setInterval(() => {
            this.simPhase += 0.05;
            const N = 128;
            const bytes = new Uint8Array(N);

            for (let i = 0; i < N; ++i) {
                const t = this.simPhase + (i / N) * 2.0;
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
