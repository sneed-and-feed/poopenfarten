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

    // 2.5. Sample Bank & Resynthesizer Controller
    const sampleSelect = document.getElementById('sampleSelect');
    const sampleBtnStrip = document.getElementById('sampleBtnStrip');
    const sampleNameBadge = document.getElementById('sampleNameBadge');
    const sampleDurationBadge = document.getElementById('sampleDurationBadge');
    const sampleReverseBtn = document.getElementById('sampleReverseBtn');
    const sampleDropBtn = document.getElementById('sampleDropBtn');
    const audioFileInput = document.getElementById('audioFileInput');
    const dropOverlay = document.getElementById('dropOverlay');

    let currentSampleIndex = 0;
    let isSampleReverse = false;

    function selectSampleIndex(idx) {
        currentSampleIndex = Math.max(0, Math.min(7, Math.round(idx)));
        if (sampleSelect) sampleSelect.value = currentSampleIndex;

        const stripBtns = document.querySelectorAll('.sample-strip-btn');
        stripBtns.forEach((btn, bIdx) => {
            if (bIdx === currentSampleIndex) {
                btn.classList.add('active');
            } else {
                btn.classList.remove('active');
            }
        });

        if (window.SampleBank) {
            const sample = window.SampleBank.getSample(currentSampleIndex);
            if (sample) {
                if (sampleNameBadge) {
                    sampleNameBadge.textContent = sample.name.toUpperCase();
                }
                if (sampleDurationBadge) {
                    const dur = (sample.numSamples / sample.sampleRate).toFixed(2);
                    sampleDurationBadge.textContent = `${dur}s (C3)`;
                }
            }
        }

        window.ipcBridge.selectSample(currentSampleIndex);
    }

    if (window.SampleBank && sampleSelect) {
        sampleSelect.innerHTML = '';
        window.SampleBank.samples.forEach((sample) => {
            const opt = document.createElement('option');
            opt.value = sample.index;
            opt.textContent = `${sample.index + 1}: ${sample.name}`;
            sampleSelect.appendChild(opt);
        });

        sampleSelect.addEventListener('change', (e) => {
            if (e.target.value === 'custom') return;
            selectSampleIndex(parseInt(e.target.value, 10));
        });
    }

    if (sampleBtnStrip) {
        const stripBtns = sampleBtnStrip.querySelectorAll('.sample-strip-btn');
        stripBtns.forEach((btn) => {
            btn.addEventListener('click', () => {
                const idx = parseInt(btn.dataset.index, 10);
                selectSampleIndex(idx);
            });
        });
    }

    if (sampleReverseBtn) {
        sampleReverseBtn.addEventListener('click', () => {
            isSampleReverse = !isSampleReverse;
            sampleReverseBtn.classList.toggle('active', isSampleReverse);
            window.ipcBridge.setSampleReverse(isSampleReverse);
        });
    }

    async function loadAudioBuffer(arrayBuffer, name) {
        const result = await window.ipcBridge.loadCustomAudio(arrayBuffer, name);
        if (result) {
            if (sampleNameBadge) {
                sampleNameBadge.textContent = result.name.toUpperCase();
            }
            if (sampleDurationBadge) {
                sampleDurationBadge.textContent = `${result.duration.toFixed(2)}s (CUSTOM)`;
            }
            document.querySelectorAll('.sample-strip-btn').forEach(btn => btn.classList.remove('active'));

            if (sampleSelect) {
                let customOpt = sampleSelect.querySelector('option[value="custom"]');
                if (!customOpt) {
                    customOpt = document.createElement('option');
                    customOpt.value = 'custom';
                    sampleSelect.appendChild(customOpt);
                }
                customOpt.textContent = `📁 ${result.name}`;
                sampleSelect.value = 'custom';
            }
        }
    }

    if (sampleDropBtn && audioFileInput) {
        sampleDropBtn.addEventListener('click', () => {
            audioFileInput.click();
        });

        audioFileInput.addEventListener('change', async (e) => {
            const file = e.target.files && e.target.files[0];
            if (!file) return;
            const arrayBuffer = await file.arrayBuffer();
            await loadAudioBuffer(arrayBuffer, file.name);
            audioFileInput.value = '';
        });
    }

    if (dropOverlay) {
        let dragCounter = 0;

        window.addEventListener('dragenter', (e) => {
            e.preventDefault();
            e.stopPropagation();
            dragCounter++;
            dropOverlay.classList.remove('hidden');
        });

        window.addEventListener('dragover', (e) => {
            e.preventDefault();
            e.stopPropagation();
        });

        window.addEventListener('dragleave', (e) => {
            e.preventDefault();
            e.stopPropagation();
            dragCounter--;
            if (dragCounter <= 0) {
                dragCounter = 0;
                dropOverlay.classList.add('hidden');
            }
        });

        window.addEventListener('drop', async (e) => {
            e.preventDefault();
            e.stopPropagation();
            dragCounter = 0;
            dropOverlay.classList.add('hidden');

            if (e.dataTransfer && e.dataTransfer.files && e.dataTransfer.files.length > 0) {
                const file = e.dataTransfer.files[0];
                const arrayBuffer = await file.arrayBuffer();
                await loadAudioBuffer(arrayBuffer, file.name);
            }
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
        if (id === 'param_sample_start') {
            return `${(num * 100).toFixed(1)}%`;
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

            if (id === 'param_sample_start') {
                window.ipcBridge.setSampleStart(val);
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
    });

    const selects = document.querySelectorAll('select.card-select');
    selects.forEach((sel) => {
        sel.addEventListener('change', (e) => {
            const id = e.target.id;
            if (id === 'sampleSelect') return; // Handled by sample controller
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

    // 6. Interactive Virtual & Musical Typing Keyboard Strip
    let currentOctave = 3; // C3 to B4
    let currentVelocity = 0.85;
    const octaveDisplay = document.getElementById('octaveDisplay');
    const octDownBtn = document.getElementById('octDownBtn');
    const octUpBtn = document.getElementById('octUpBtn');
    const velDisplay = document.getElementById('velDisplay');
    const velDownBtn = document.getElementById('velDownBtn');
    const velUpBtn = document.getElementById('velUpBtn');
    const panicBtn = document.getElementById('panicBtn');
    const exciteBurstBtn = document.getElementById('exciteBurstBtn');

    function updateOctave(delta) {
        currentOctave = Math.max(1, Math.min(6, currentOctave + delta));
        if (octaveDisplay) octaveDisplay.textContent = `C${currentOctave}`;
        buildKeyboard();
    }

    function updateVelocity(delta) {
        currentVelocity = Math.max(0.1, Math.min(1.0, currentVelocity + delta));
        if (velDisplay) velDisplay.textContent = `${Math.round(currentVelocity * 100)}%`;
    }

    if (octDownBtn) octDownBtn.addEventListener('click', () => updateOctave(-1));
    if (octUpBtn) octUpBtn.addEventListener('click', () => updateOctave(1));
    if (velDownBtn) velDownBtn.addEventListener('click', () => updateVelocity(-0.1));
    if (velUpBtn) velUpBtn.addEventListener('click', () => updateVelocity(0.1));

    if (panicBtn) {
        panicBtn.addEventListener('click', () => {
            window.ipcBridge.allNotesOff();
            document.querySelectorAll('.piano-keys .active').forEach(k => k.classList.remove('active'));
            activeNotes.clear();
        });
    }

    // Excite / Colonic Pressure Burst
    let burstTimer = null;
    function triggerExciteBurst() {
        if (exciteBurstBtn) exciteBurstBtn.classList.add('firing');
        const baseNote = (currentOctave + 1) * 12;
        // Pitch variation around root C for visceral punch
        const burstNote = baseNote;
        window.ipcBridge.noteOn(burstNote, 1.0);

        const keyElem = document.querySelector(`.piano-keys [data-note="${burstNote}"]`);
        if (keyElem) keyElem.classList.add('active');

        if (burstTimer) clearTimeout(burstTimer);
        burstTimer = setTimeout(() => {
            window.ipcBridge.noteOff(burstNote, 0.0);
            if (keyElem) keyElem.classList.remove('active');
            if (exciteBurstBtn) exciteBurstBtn.classList.remove('firing');
        }, 160);
    }

    if (exciteBurstBtn) {
        exciteBurstBtn.addEventListener('click', triggerExciteBurst);
    }

    const keyboardContainer = document.getElementById('pianoKeys');
    let activeNotes = new Set();

    const triggerNoteOn = (note) => {
        if (!activeNotes.has(note)) {
            activeNotes.add(note);
            window.ipcBridge.noteOn(note, currentVelocity);
            const key = document.querySelector(`.piano-keys [data-note="${note}"]`);
            if (key) key.classList.add('active');
        }
    };

    const triggerNoteOff = (note) => {
        if (activeNotes.has(note)) {
            activeNotes.delete(note);
            window.ipcBridge.noteOff(note);
            const key = document.querySelector(`.piano-keys [data-note="${note}"]`);
            if (key) key.classList.remove('active');
        }
    };

    // Computer typing key to semitone offset map
    const TYPING_KEY_MAP = {
        'a': 0,  'w': 1,  's': 2,  'e': 3,  'd': 4,  'f': 5,  't': 6,
        'g': 7,  'y': 8,  'h': 9,  'u': 10, 'j': 11, 'k': 12, 'o': 13,
        'l': 14, 'p': 15, ';': 16, "'": 17
    };

    function buildKeyboard() {
        if (!keyboardContainer) return;
        keyboardContainer.innerHTML = '';

        const baseNote = (currentOctave + 1) * 12; // C3 = 48 if octave=3
        const whiteOffsets = [
            { semitone: 0,  noteName: `C${currentOctave}`,     key: 'A' },
            { semitone: 2,  noteName: `D${currentOctave}`,     key: 'S' },
            { semitone: 4,  noteName: `E${currentOctave}`,     key: 'D' },
            { semitone: 5,  noteName: `F${currentOctave}`,     key: 'F' },
            { semitone: 7,  noteName: `G${currentOctave}`,     key: 'G' },
            { semitone: 9,  noteName: `A${currentOctave}`,     key: 'H' },
            { semitone: 11, noteName: `B${currentOctave}`,     key: 'J' },
            { semitone: 12, noteName: `C${currentOctave + 1}`, key: 'K' },
            { semitone: 14, noteName: `D${currentOctave + 1}`, key: 'L' },
            { semitone: 16, noteName: `E${currentOctave + 1}`, key: ';' },
            { semitone: 17, noteName: `F${currentOctave + 1}`, key: "'" },
            { semitone: 19, noteName: `G${currentOctave + 1}`, key: '' },
            { semitone: 21, noteName: `A${currentOctave + 1}`, key: '' },
            { semitone: 23, noteName: `B${currentOctave + 1}`, key: '' }
        ];

        const blackDefs = [
            { semitone: 1,  whiteIdx: 0,  noteName: `C#${currentOctave}`,     key: 'W' },
            { semitone: 3,  whiteIdx: 1,  noteName: `D#${currentOctave}`,     key: 'E' },
            { semitone: 6,  whiteIdx: 3,  noteName: `F#${currentOctave}`,     key: 'T' },
            { semitone: 8,  whiteIdx: 4,  noteName: `G#${currentOctave}`,     key: 'Y' },
            { semitone: 10, whiteIdx: 5,  noteName: `A#${currentOctave}`,     key: 'U' },
            { semitone: 13, whiteIdx: 7,  noteName: `C#${currentOctave + 1}`, key: 'O' },
            { semitone: 15, whiteIdx: 8,  noteName: `D#${currentOctave + 1}`, key: 'P' },
            { semitone: 18, whiteIdx: 10, noteName: `F#${currentOctave + 1}`, key: '' },
            { semitone: 20, whiteIdx: 11, noteName: `G#${currentOctave + 1}`, key: '' },
            { semitone: 22, whiteIdx: 12, noteName: `A#${currentOctave + 1}`, key: '' }
        ];

        // 1. Render White Keys
        for (let i = 0; i < whiteOffsets.length; ++i) {
            const def = whiteOffsets[i];
            const note = baseNote + def.semitone;
            const key = document.createElement('div');
            key.className = 'white-key';
            key.dataset.note = note;

            if (def.noteName) {
                const noteSpan = document.createElement('span');
                noteSpan.className = 'key-note';
                noteSpan.textContent = def.noteName;
                key.appendChild(noteSpan);
            }

            if (def.key) {
                const badge = document.createElement('span');
                badge.className = 'key-label';
                badge.textContent = def.key;
                key.appendChild(badge);
            }

            key.addEventListener('mousedown', () => triggerNoteOn(note));
            key.addEventListener('mouseup', () => triggerNoteOff(note));
            key.addEventListener('mouseleave', () => triggerNoteOff(note));

            keyboardContainer.appendChild(key);
        }

        // 2. Render Black Keys
        const whiteKeyWidthPercent = 100 / whiteOffsets.length;
        blackDefs.forEach(def => {
            const note = baseNote + def.semitone;
            const key = document.createElement('div');
            key.className = 'black-key';
            key.dataset.note = note;
            key.style.left = `${(def.whiteIdx + 1) * whiteKeyWidthPercent - 1.7}%`;

            if (def.noteName) {
                const noteSpan = document.createElement('span');
                noteSpan.className = 'key-note';
                noteSpan.textContent = def.noteName;
                key.appendChild(noteSpan);
            }

            if (def.key) {
                const badge = document.createElement('span');
                badge.className = 'key-label';
                badge.textContent = def.key;
                key.appendChild(badge);
            }

            key.addEventListener('mousedown', (e) => {
                e.stopPropagation();
                triggerNoteOn(note);
            });
            key.addEventListener('mouseup', (e) => {
                e.stopPropagation();
                triggerNoteOff(note);
            });
            key.addEventListener('mouseleave', (e) => {
                e.stopPropagation();
                triggerNoteOff(note);
            });

            keyboardContainer.appendChild(key);
        });
    }
    buildKeyboard();

    // 7. Global Computer Keyboard Listeners (Musical Typing)
    window.addEventListener('keydown', (e) => {
        // Ignore typing when focused inside an input or select
        if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;
        if (e.repeat) return;

        const k = e.key.toLowerCase();

        if (e.code === 'Space') {
            e.preventDefault();
            triggerExciteBurst();
            return;
        }

        if (k === 'z') {
            updateOctave(-1);
            return;
        }
        if (k === 'x') {
            updateOctave(1);
            return;
        }
        if (k === 'c') {
            updateVelocity(-0.1);
            return;
        }
        if (k === 'v') {
            updateVelocity(0.1);
            return;
        }
        if (e.key === 'Escape') {
            window.ipcBridge.allNotesOff();
            document.querySelectorAll('.piano-keys .active').forEach(el => el.classList.remove('active'));
            activeNotes.clear();
            return;
        }

        if (k in TYPING_KEY_MAP) {
            e.preventDefault();
            const baseNote = (currentOctave + 1) * 12;
            const note = baseNote + TYPING_KEY_MAP[k];
            triggerNoteOn(note);
        }
    });

    window.addEventListener('keyup', (e) => {
        if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;

        const k = e.key.toLowerCase();
        if (k in TYPING_KEY_MAP) {
            e.preventDefault();
            const baseNote = (currentOctave + 1) * 12;
            const note = baseNote + TYPING_KEY_MAP[k];
            triggerNoteOff(note);
        }
    });

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
        if (select && select.tagName === 'SELECT' && id !== 'sampleSelect') {
            select.value = Math.round(val);
        }

        if (id === 'sample_index') {
            selectSampleIndex(val);
        } else if (id === 'sample_start') {
            const startSlider = document.getElementById('param_sample_start');
            if (startSlider) {
                startSlider.value = val;
                updateLabel('param_sample_start', val);
            }
        } else if (id === 'sample_reverse') {
            isSampleReverse = !!val;
            if (sampleReverseBtn) {
                sampleReverseBtn.classList.toggle('active', isSampleReverse);
            }
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
                if (select && select.tagName === 'SELECT' && id !== 'sampleSelect') {
                    select.value = Math.round(val);
                }
            }

            if (typeof data.params['sample_index'] !== 'undefined') {
                selectSampleIndex(data.params['sample_index']);
            }
            if (typeof data.params['sample_start'] !== 'undefined') {
                const sVal = data.params['sample_start'];
                const startSlider = document.getElementById('param_sample_start');
                if (startSlider) {
                    startSlider.value = sVal;
                    updateLabel('param_sample_start', sVal);
                }
            }
            if (typeof data.params['sample_reverse'] !== 'undefined') {
                isSampleReverse = !!data.params['sample_reverse'];
                if (sampleReverseBtn) {
                    sampleReverseBtn.classList.toggle('active', isSampleReverse);
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
