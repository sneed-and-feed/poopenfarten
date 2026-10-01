/**
 * PPF-42 DYNAMICS - Phosphor Spectral Oscilloscope
 * Decodes Base64-serialized 128-sample relaxation waveforms
 * and renders a high-definition phosphor vector trace at 60 FPS.
 */

class SpectralScopeVisualizer {
    constructor(canvasId) {
        this.canvas = document.getElementById(canvasId);
        if (!this.canvas) return;
        this.ctx = this.canvas.getContext('2d');

        this.numSamples = 128;
        this.samples = new Float32Array(this.numSamples);
        this.rmsL = 0.0;
        this.rmsR = 0.0;

        this.dpr = window.devicePixelRatio || 1;
        this.width = 0;
        this.height = 0;

        this._bindResize();
        this._startLoop();
    }

    _bindResize() {
        const resize = () => {
            const rect = this.canvas.getBoundingClientRect();
            this.dpr = window.devicePixelRatio || 1;
            this.width = Math.round(rect.width);
            this.height = Math.round(rect.height);

            const targetW = Math.floor(this.width * this.dpr);
            const targetH = Math.floor(this.height * this.dpr);

            if (this.canvas.width !== targetW || this.canvas.height !== targetH) {
                this.canvas.width = targetW;
                this.canvas.height = targetH;
            }
        };

        window.addEventListener('resize', resize);
        const observer = new ResizeObserver(resize);
        observer.observe(this.canvas);
        resize();
    }

    updateTelemetry(frame) {
        if (!frame) return;

        if (typeof frame.waveform === 'string' && frame.waveform.length > 0) {
            try {
                const raw = atob(frame.waveform);
                const len = Math.min(this.numSamples, raw.length);
                const inv127 = 1.0 / 127.0;
                for (let i = 0; i < len; ++i) {
                    this.samples[i] = (raw.charCodeAt(i) - 128) * inv127;
                }
            } catch (err) {
                // Invalid Base64 ignored
            }
        }

        if (typeof frame.rmsL === 'number') this.rmsL = frame.rmsL;
        if (typeof frame.rmsR === 'number') this.rmsR = frame.rmsR;
    }

    _startLoop() {
        const render = () => {
            this._draw();
            requestAnimationFrame(render);
        };
        requestAnimationFrame(render);
    }

    _draw() {
        if (!this.ctx || this.width <= 0 || this.height <= 0) return;

        const ctx = this.ctx;
        ctx.save();
        ctx.scale(this.dpr, this.dpr);

        // Dark slate background
        ctx.fillStyle = '#0d1117';
        ctx.fillRect(0, 0, this.width, this.height);

        const cy = this.height * 0.5;

        // 1. Graticule Grid Lines
        ctx.lineWidth = 1;
        ctx.strokeStyle = '#1e242c';
        ctx.beginPath();
        for (let x = 0; x < this.width; x += 40) {
            ctx.moveTo(x, 0);
            ctx.lineTo(x, this.height);
        }
        for (let y = 0; y < this.height; y += 30) {
            ctx.moveTo(0, y);
            ctx.lineTo(this.width, y);
        }
        ctx.stroke();

        // Center zero-crossing line
        ctx.strokeStyle = '#30363d';
        ctx.setLineDash([4, 4]);
        ctx.beginPath();
        ctx.moveTo(0, cy);
        ctx.lineTo(this.width, cy);
        ctx.stroke();
        ctx.setLineDash([]);

        // 2. Phosphor Vector Waveform Trace
        const step = this.width / (this.numSamples - 1);
        const amp = this.height * 0.44;

        // Pass 1: Outer Soft Glow
        ctx.save();
        ctx.shadowBlur = 10;
        ctx.shadowColor = '#58a6ff';
        ctx.strokeStyle = 'rgba(88, 166, 255, 0.4)';
        ctx.lineWidth = 3;
        ctx.beginPath();
        for (let i = 0; i < this.numSamples; ++i) {
            const x = i * step;
            const y = cy - this.samples[i] * amp;
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.stroke();
        ctx.restore();

        // Pass 2: Sharp Core Trace
        ctx.strokeStyle = '#58a6ff';
        ctx.lineWidth = 1.5;
        ctx.beginPath();
        for (let i = 0; i < this.numSamples; ++i) {
            const x = i * step;
            const y = cy - this.samples[i] * amp;
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.stroke();

        // 3. Stereo RMS Readout & Scope Metadata
        ctx.fillStyle = '#8b949e';
        ctx.font = '10px monospace';
        ctx.fillText('SPECTRUM / OSCILLOSCOPE', 12, 18);
        ctx.fillText(`L: ${(20 * Math.log10(Math.max(0.0001, this.rmsL))).toFixed(1)} dB`, this.width - 95, 18);
        ctx.fillText(`R: ${(20 * Math.log10(Math.max(0.0001, this.rmsR))).toFixed(1)} dB`, this.width - 95, 32);

        ctx.restore();
    }
}

window.SpectralScopeVisualizer = SpectralScopeVisualizer;
