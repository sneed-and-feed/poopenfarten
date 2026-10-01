/**
 * PPF-42 DYNAMICS - Aeroacoustic Particle Chamber Visualizer
 * Simulates real-time fluid dynamics, Bernoulli aperture constriction,
 * Reynolds turbulent vortex shedding, and Minnaert moisture splatter.
 */

class ParticleChamberVisualizer {
    constructor(canvasId) {
        this.canvas = document.getElementById(canvasId);
        if (!this.canvas) return;
        this.ctx = this.canvas.getContext('2d');

        this.particles = [];
        this.droplets = [];
        this.maxParticles = 180;
        this.maxDroplets = 40;

        this.currentAperture = 0.35;
        this.currentVelocity = 12.0;
        this.currentPressure = 0.70;
        this.currentMoisture = 0.25;

        this.dpr = window.devicePixelRatio || 1;
        this.width = 0;
        this.height = 0;

        this._initParticles();
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

    _initParticles() {
        for (let i = 0; i < this.maxParticles; ++i) {
            this.particles.push(this._createParticle(true));
        }
    }

    _createParticle(randomX = false) {
        const x = randomX ? Math.random() * (this.width || 300) : (this.width * 0.22);
        const y = (this.height * 0.5) + (Math.random() - 0.5) * (this.height * 0.3 * Math.max(0.1, this.currentAperture));
        const speed = 1.5 + Math.random() * (this.currentVelocity * 0.15 + 2.0);

        return {
            x,
            y,
            vx: speed,
            vy: (Math.random() - 0.5) * (speed * 0.6),
            life: Math.random() * 0.8 + 0.2,
            maxLife: 1.0,
            size: Math.random() * 2.5 + 1.2,
            hue: Math.random() > 0.4 ? 'cyan' : 'amber'
        };
    }

    _createDroplet() {
        const speed = 3.0 + Math.random() * 5.0;
        const angle = (Math.random() - 0.5) * 1.2;
        return {
            x: this.width * 0.22,
            y: (this.height * 0.5) + (Math.random() - 0.5) * (this.height * 0.2 * this.currentAperture),
            vx: Math.cos(angle) * speed,
            vy: Math.sin(angle) * speed,
            life: 1.0,
            decay: 0.02 + Math.random() * 0.03,
            radius: Math.random() * 3.5 + 2.0
        };
    }

    updateTelemetry(frame) {
        if (!frame) return;
        if (typeof frame.aperture === 'number') this.currentAperture = frame.aperture;
        if (typeof frame.velocity === 'number') this.currentVelocity = frame.velocity;
        if (typeof frame.pressure === 'number') this.currentPressure = frame.pressure;
        if (typeof frame.bubbleDensity === 'number') this.currentMoisture = frame.bubbleDensity;

        if (frame.dropletPops > 0.05 || (Math.random() < this.currentMoisture * 0.15)) {
            const count = Math.min(4, Math.floor(frame.dropletPops * 3) + 1);
            for (let i = 0; i < count; ++i) {
                if (this.droplets.length < this.maxDroplets) {
                    this.droplets.push(this._createDroplet());
                }
            }
        }
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

        // Dark background clearing with motion trail
        ctx.fillStyle = 'rgba(13, 17, 23, 0.35)';
        ctx.fillRect(0, 0, this.width, this.height);

        const cx = this.width * 0.22;
        const cy = this.height * 0.5;
        const gap = Math.max(4, this.height * 0.45 * this.currentAperture);

        // 1. Draw Constriction Valve Lips
        ctx.strokeStyle = '#30363d';
        ctx.lineWidth = 3;
        ctx.fillStyle = 'rgba(22, 27, 34, 0.9)';

        // Top lip
        ctx.beginPath();
        ctx.moveTo(cx - 30, 0);
        ctx.bezierCurveTo(cx - 10, cy - gap, cx + 10, cy - gap, cx + 30, 0);
        ctx.closePath();
        ctx.fill();
        ctx.stroke();

        // Bottom lip
        ctx.beginPath();
        ctx.moveTo(cx - 30, this.height);
        ctx.bezierCurveTo(cx - 10, cy + gap, cx + 10, cy + gap, cx + 30, this.height);
        ctx.closePath();
        ctx.fill();
        ctx.stroke();

        // Lip contour glow
        ctx.strokeStyle = 'rgba(88, 166, 255, 0.4)';
        ctx.lineWidth = 1.5;
        ctx.beginPath();
        ctx.moveTo(cx - 20, cy - gap);
        ctx.lineTo(cx + 20, cy - gap);
        ctx.moveTo(cx - 20, cy + gap);
        ctx.lineTo(cx + 20, cy + gap);
        ctx.stroke();

        // 2. Update & Draw Aeroacoustic Air Particles
        for (let i = 0; i < this.particles.length; ++i) {
            const p = this.particles[i];
            p.x += p.vx;
            p.y += p.vy;

            // Turbulent vortex shedding expansion downstream
            if (p.x > cx) {
                p.vy += (Math.random() - 0.5) * 0.4;
                p.vx *= 0.995;
            }

            p.life -= 0.008;

            if (p.x > this.width || p.y < 0 || p.y > this.height || p.life <= 0) {
                this.particles[i] = this._createParticle(false);
                continue;
            }

            const alpha = Math.max(0, Math.min(1, p.life));
            ctx.beginPath();
            ctx.arc(p.x, p.y, p.size, 0, Math.PI * 2);

            if (p.hue === 'cyan') {
                ctx.fillStyle = `rgba(88, 166, 255, ${alpha * 0.85})`;
            } else {
                ctx.fillStyle = `rgba(240, 136, 62, ${alpha * 0.75})`;
            }
            ctx.fill();
        }

        // 3. Update & Draw Minnaert Moisture Splatter Droplets
        for (let i = this.droplets.length - 1; i >= 0; --i) {
            const d = this.droplets[i];
            d.x += d.vx;
            d.y += d.vy;
            d.vy += 0.15; // Gravity acceleration
            d.life -= d.decay;

            if (d.life <= 0 || d.x > this.width || d.y > this.height) {
                this.droplets.splice(i, 1);
                continue;
            }

            ctx.save();
            ctx.shadowBlur = 6;
            ctx.shadowColor = '#39d353';
            ctx.fillStyle = `rgba(57, 211, 83, ${d.life * 0.9})`;
            ctx.beginPath();
            ctx.arc(d.x, d.y, d.radius * d.life, 0, Math.PI * 2);
            ctx.fill();
            ctx.restore();
        }

        // 4. Chamber Metadata Overlay
        ctx.fillStyle = '#8b949e';
        ctx.font = '10px monospace';
        ctx.fillText(`AIRFLOW: ${(this.currentVelocity * 3.4).toFixed(1)} m/s`, 12, 18);
        ctx.fillText(`APERTURE: ${(this.currentAperture * 100).toFixed(0)}%`, 12, 32);

        ctx.restore();
    }
}

window.ParticleChamberVisualizer = ParticleChamberVisualizer;
