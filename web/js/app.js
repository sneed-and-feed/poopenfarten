/**
 * PPF-42 DYNAMICS - Main Application Controller
 * Handles UI interactions, 2D XY Performance Pad, ADSR Envelope rendering,
 * Preset switching, Virtual Keyboard, and two-way IPC event binding.
 */

document.addEventListener('DOMContentLoaded', () => {
    // 1. Initialize Visualizers
    const particleVisualizer = new ParticleChamberVisualizer('particlesCanvas');
    const scopeVisualizer = new SpectralScopeVisualizer('scopeCanvas');

    // 2. Preset Definitions
    const PRESETS = [
        "01 - Clean Continental Purr",
        "02 - High-Tension Squeaker",
        "03 - Viscous Multiphase Splatter",
        "04 - Visceral Sub-Rumble (18 Hz)",
        "05 - Flutter-Tongue Stutter",
        "06 - Wet Porcelain Slam",
        "07 - Micro-Puff Staccato",
        "08 - Extended Gaseous Drift",
        "09 - Unison Twin Cannons",
        "10 - The Brown Note 808"
    ];

    const presetSelect = document.getElementById('presetSelect');
    const prevPresetBtn = document.getElementById('prevPresetBtn');
    const nextPresetBtn = document.getElementById('nextPresetBtn');

    if (presetSelect) {
        presetSelect.innerHTML = '';
        PRESETS.forEach((name, idx) => {
            const opt = document.createElement('option');
            opt.value = idx;
            opt.textContent = name;
            presetSelect.appendChild(opt);
        });

        presetSelect.addEventListener('change', (e) => {
            const idx = parseInt(e.target.value, 10);
            window.ipcBridge.loadPreset(idx);
        });
    }

    if (prevPresetBtn && presetSelect) {
        prevPresetBtn.addEventListener('click', () => {
            let idx = parseInt(presetSelect.value, 10) - 1;
            if (idx < 0) idx = PRESETS.length - 1;
            presetSelect.value = idx;
            window.ipcBridge.loadPreset(idx);
        });
    }

    if (nextPresetBtn && presetSelect) {
        nextPresetBtn.addEventListener('click', () => {
            let idx = (parseInt(presetSelect.value, 10) + 1) % PRESETS.length;
            presetSelect.value = idx;
            window.ipcBridge.loadPreset(idx);
        });
    }

    // 3. 2D XY Performance Pad (Gut Squeeze vs Dietary Moisture)
    const xyPad = document.getElementById('xyPad');
    const xyPuck = document.getElementById('xyPuck');
    const xyCoords = document.getElementById('xyCoords');
    let isDraggingPad = false;

    function updateXYPad(xNorm, yNorm, emitIpc = true) {
        const x = Math.max(0, Math.min(1, xNorm));
        const y = Math.max(0, Math.min(1, yNorm));

        if (xyPuck && xyPad) {
            const rect = xyPad.getBoundingClientRect();
            xyPuck.style.left = `${x * 100}%`;
            xyPuck.style.top = `${(1 - y) * 100}%`;
        }

        if (xyCoords) {
            xyCoords.textContent = `SQ: ${(x * 100).toFixed(0)}% | MOIST: ${(y * 100).toFixed(0)}%`;
        }

        // Update corresponding sliders if present
        const squeezeInput = document.getElementById('macro_squeeze');
        const moistureInput = document.getElementById('macro_moisture');
        if (squeezeInput) {
            squeezeInput.value = x;
            updateLabel('macro_squeeze', x);
        }
        if (moistureInput) {
            moistureInput.value = y;
            updateLabel('macro_moisture', y);
        }

        if (emitIpc) {
            window.ipcBridge.setParameter('macro_squeeze', x);
            window.ipcBridge.setParameter('macro_moisture', y);
        }
    }

    if (xyPad) {
        const handlePadEvent = (e) => {
            const rect = xyPad.getBoundingClientRect();
            const clientX = e.clientX || (e.touches && e.touches[0].clientX);
            const clientY = e.clientY || (e.touches && e.touches[0].clientY);
            if (clientX === undefined || clientY === undefined) return;

            const x = (clientX - rect.left) / rect.width;
            const y = 1.0 - ((clientY - rect.top) / rect.height);
            updateXYPad(x, y, true);
        };

        xyPad.addEventListener('mousedown', (e) => {
            isDraggingPad = true;
            handlePadEvent(e);
        });
        window.addEventListener('mousemove', (e) => {
            if (isDraggingPad) handlePadEvent(e);
        });
        window.addEventListener('mouseup', () => {
            isDraggingPad = false;
        });

        xyPad.addEventListener('touchstart', (e) => {
            isDraggingPad = true;
            handlePadEvent(e);
            e.preventDefault();
        });
        window.addEventListener('touchmove', (e) => {
            if (isDraggingPad) {
                handlePadEvent(e);
                e.preventDefault();
            }
        });
        window.addEventListener('touchend', () => {
            isDraggingPad = false;
        });
    }

    // 4. Parameter Sliders & Value Formatters
    function formatValue(id, val) {
        const num = parseFloat(val);
        if (id.includes('time') || id.includes('attack') || id.includes('decay') || id.includes('release')) {
            return `${num.toFixed(1)} ms`;
        }
        if (id.includes('level') || id.includes('gain')) {
            return `${num >= 0 ? '+' : ''}${num.toFixed(1)} dB`;
        }
        if (id.includes('size')) {
            return `${num.toFixed(2)}x`;
        }
        if (id === 'param_porcelain_model') {
            const models = ['Dry Chamber', 'Ceramic Bowl', 'Water Coupled', 'Tiled Enclosure'];
            return models[Math.round(num)] || 'Model';
        }
        if (id === 'param_voice_mode') {
            const modes = ['Mono Legato', 'Poly 8-Voice', 'Stereo Unison'];
            return modes[Math.round(num)] || 'Mode';
        }
        return `${(num * 100).toFixed(0)}%`;
    }

    function updateLabel(id, val) {
        const valElem = document.getElementById(`${id}_val`);
        if (valElem) {
            valElem.textContent = formatValue(id, val);
        }
    }

    const sliders = document.querySelectorAll('input[type="range"]');
    sliders.forEach((slider) => {
        slider.addEventListener('input', (e) => {
            const id = e.target.id;
            const val = parseFloat(e.target.value);
            updateLabel(id, val);
            window.ipcBridge.setParameter(id, val);

            if (id === 'macro_squeeze') {
                const moist = parseFloat(document.getElementById('macro_moisture')?.value || 0.2);
                updateXYPad(val, moist, false);
            } else if (id === 'macro_moisture') {
                const sq = parseFloat(document.getElementById('macro_squeeze')?.value || 0.5);
                updateXYPad(sq, val, false);
            }

            if (id.startsWith('param_env_')) {
                renderAdsr();
            }
        });
    });

    const selects = document.querySelectorAll('select.card-select');
    selects.forEach((sel) => {
        sel.addEventListener('change', (e) => {
            const id = e.target.id;
            const val = parseFloat(e.target.value);
            window.ipcBridge.setParameter(id, val);
        });
    });

    // 5. ADSR Envelope Curve Preview
    const adsrCanvas = document.getElementById('adsrCanvas');
    function renderAdsr() {
        if (!adsrCanvas) return;
        const ctx = adsrCanvas.getContext('2d');
        const dpr = window.devicePixelRatio || 1;
        const rect = adsrCanvas.getBoundingClientRect();
        adsrCanvas.width = Math.floor(rect.width * dpr);
        adsrCanvas.height = Math.floor(rect.height * dpr);

        ctx.save();
        ctx.scale(dpr, dpr);
        ctx.fillStyle = '#090b0e';
        ctx.fillRect(0, 0, rect.width, rect.height);

        const a = parseFloat(document.getElementById('param_env_attack')?.value || 5.0);
        const d = parseFloat(document.getElementById('param_env_decay')?.value || 350.0);
        const s = parseFloat(document.getElementById('param_env_sustain')?.value || 0.6);
        const r = parseFloat(document.getElementById('param_env_release')?.value || 120.0);

        const totalTime = a + d + 300 + r;
        const w = rect.width;
        const h = rect.height - 8;

        const xA = (a / totalTime) * w;
        const xD = xA + (d / totalTime) * w;
        const xS = xD + (300 / totalTime) * w;
        const xR = w;

        const yPeak = 4;
        const ySustain = h - (s * (h - 4)) + 4;
        const yBase = h + 4;

        ctx.lineWidth = 2;
        ctx.strokeStyle = '#f0883e';
        ctx.shadowBlur = 6;
        ctx.shadowColor = '#f0883e';

        ctx.beginPath();
        ctx.moveTo(0, yBase);
        ctx.lineTo(xA, yPeak);
        ctx.lineTo(xD, ySustain);
        ctx.lineTo(xS, ySustain);
        ctx.lineTo(xR, yBase);
        ctx.stroke();

        ctx.restore();
    }
    renderAdsr();

    // 6. Interactive Virtual Keyboard Strip
    let currentOctave = 3; // C3 to B4
    const octaveDisplay = document.getElementById('octaveDisplay');
    const octDownBtn = document.getElementById('octDownBtn');
    const octUpBtn = document.getElementById('octUpBtn');

    function updateOctave(delta) {
        currentOctave = Math.max(1, Math.min(6, currentOctave + delta));
        if (octaveDisplay) octaveDisplay.textContent = `OCT: C${currentOctave}`;
        buildKeyboard();
    }

    if (octDownBtn) octDownBtn.addEventListener('click', () => updateOctave(-1));
    if (octUpBtn) octUpBtn.addEventListener('click', () => updateOctave(1));

    const keyboardContainer = document.getElementById('pianoKeys');
    function buildKeyboard() {
        if (!keyboardContainer) return;
        keyboardContainer.innerHTML = '';

        const baseNote = (currentOctave + 1) * 12; // C3 = 48 if octave=3
        const numWhiteKeys = 14; // 2 octaves of white keys
        const whiteKeyNotes = [];

        // Build 2 octaves: C, D, E, F, G, A, B, C, D, E, F, G, A, B
        const whiteOffsets = [0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23];
        const blackDefs = [
            { offset: 1, whiteIdx: 0 },
            { offset: 3, whiteIdx: 1 },
            { offset: 6, whiteIdx: 3 },
            { offset: 8, whiteIdx: 4 },
            { offset: 10, whiteIdx: 5 },
            { offset: 13, whiteIdx: 7 },
            { offset: 15, whiteIdx: 8 },
            { offset: 18, whiteIdx: 10 },
            { offset: 20, whiteIdx: 11 },
            { offset: 22, whiteIdx: 12 }
        ];

        let activeNotes = new Set();

        const triggerNoteOn = (note) => {
            if (!activeNotes.has(note)) {
                activeNotes.add(note);
                window.ipcBridge.noteOn(note, 0.85);
            }
        };

        const triggerNoteOff = (note) => {
            if (activeNotes.has(note)) {
                activeNotes.delete(note);
                window.ipcBridge.noteOff(note);
            }
        };

        for (let i = 0; i < whiteOffsets.length; ++i) {
            const note = baseNote + whiteOffsets[i];
            const key = document.createElement('div');
            key.className = 'white-key';
            key.dataset.note = note;

            key.addEventListener('mousedown', () => { key.classList.add('active'); triggerNoteOn(note); });
            key.addEventListener('mouseup', () => { key.classList.remove('active'); triggerNoteOff(note); });
            key.addEventListener('mouseleave', () => { key.classList.remove('active'); triggerNoteOff(note); });

            keyboardContainer.appendChild(key);
        }

        // Add black keys overlaid
        const whiteKeyWidthPercent = 100 / whiteOffsets.length;
        blackDefs.forEach(({ offset, whiteIdx }) => {
            const note = baseNote + offset;
            const key = document.createElement('div');
            key.className = 'black-key';
            key.dataset.note = note;
            key.style.left = `${(whiteIdx + 1) * whiteKeyWidthPercent - 1.6}%`;

            key.addEventListener('mousedown', (e) => {
                e.stopPropagation();
                key.classList.add('active');
                triggerNoteOn(note);
            });
            key.addEventListener('mouseup', (e) => {
                e.stopPropagation();
                key.classList.remove('active');
                triggerNoteOff(note);
            });
            key.addEventListener('mouseleave', (e) => {
                e.stopPropagation();
                key.classList.remove('active');
                triggerNoteOff(note);
            });

            keyboardContainer.appendChild(key);
        });
    }
    buildKeyboard();

    // 7. IPC Telemetry & Parameter Listeners
    window.ipcBridge.addEventListener('visualizerFrame', (frame) => {
        particleVisualizer.updateTelemetry(frame);
        scopeVisualizer.updateTelemetry(frame);
    });

    window.ipcBridge.addEventListener('paramUpdate', (data) => {
        if (!data || !data.id) return;
        const id = data.id;
        const val = parseFloat(data.value);

        const slider = document.getElementById(id);
        if (slider) {
            slider.value = val;
            updateLabel(id, val);
        }

        const select = document.getElementById(id);
        if (select && select.tagName === 'SELECT') {
            select.value = Math.round(val);
        }

        if (id === 'macro_squeeze') {
            const moist = parseFloat(document.getElementById('macro_moisture')?.value || 0.2);
            updateXYPad(val, moist, false);
        } else if (id === 'macro_moisture') {
            const sq = parseFloat(document.getElementById('macro_squeeze')?.value || 0.5);
            updateXYPad(sq, val, false);
        }

        if (id.startsWith('param_env_')) {
            renderAdsr();
        }
    });

    window.ipcBridge.addEventListener('stateSync', (data) => {
        if (!data) return;

        if (typeof data.currentPreset === 'number' && presetSelect) {
            presetSelect.value = data.currentPreset;
        }

        if (data.params) {
            for (const [id, val] of Object.entries(data.params)) {
                const slider = document.getElementById(id);
                if (slider) {
                    slider.value = val;
                    updateLabel(id, val);
                }
                const select = document.getElementById(id);
                if (select && select.tagName === 'SELECT') {
                    select.value = Math.round(val);
                }
            }

            const sq = parseFloat(data.params['macro_squeeze'] ?? 0.5);
            const mm = parseFloat(data.params['macro_moisture'] ?? 0.2);
            updateXYPad(sq, mm, false);
            renderAdsr();
        }
    });

    // Request initial state synchronization
    window.ipcBridge.requestState();
});
