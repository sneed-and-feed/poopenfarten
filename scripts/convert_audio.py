import os
import subprocess
import struct
import base64
import math

SAMPLES = [
    {
        "id": "dry_fart",
        "file": "dry-fart.mp3",
        "name": "Dry Staccato Rip",
        "desc": "Crisp dry staccato tear with rapid mucosal release"
    },
    {
        "id": "extended_gaseous",
        "file": "faaaaaaaaaaaaaaaaaaaaaaaaaaaaaaart.mp3",
        "name": "Extended Gaseous Rip",
        "desc": "Sustained aerodynamic flapper with rich harmonic drone"
    },
    {
        "id": "porcelain_slam",
        "file": "fart-with-reverb.mp3",
        "name": "Porcelain Room Slam",
        "desc": "Heavy explosive detonation with cavernous tiled bathroom reverb"
    },
    {
        "id": "iconic_meme",
        "file": "fartmeme.mp3",
        "name": "Iconic Wet Meme (4gcs5k8n-FY)",
        "desc": "The legendary classic YouTube wet fart sound effect"
    },
    {
        "id": "juicy_squelch",
        "file": "fart_2.mp3",
        "name": "Juicy Squelch Pop",
        "desc": "Viscous fluid ejection with bubbling transient snap"
    },
    {
        "id": "flapping_flutter",
        "file": "fooz.mp3",
        "name": "Flapping Flutter",
        "desc": "High-velocity fluttering tissue vibration with turbulent hiss"
    },
    {
        "id": "crisp_slap",
        "file": "perfect-fart.mp3",
        "name": "Crisp Percussive Slap",
        "desc": "Ultra-tight percussive thud with immediate cutoff"
    },
    {
        "id": "multiphase_splatter",
        "file": "wet-fart-2.mp3",
        "name": "Multiphase Splatter",
        "desc": "Chaotic multi-stage fluid discharge with resonant splatter"
    }
]

def main():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    brap_dir = os.path.join(repo_root, "brap")
    wav_dir = os.path.join(brap_dir, "wav")
    os.makedirs(wav_dir, exist_ok=True)

    print(f"Working in repository: {repo_root}")
    print(f"Decoding {len(SAMPLES)} audio files into {wav_dir}...")

    processed_samples = []

    for idx, s in enumerate(SAMPLES):
        mp3_path = os.path.join(brap_dir, s["file"])
        raw_wav_path = os.path.join(wav_dir, f"temp_{idx}.wav")
        norm_wav_path = os.path.join(wav_dir, os.path.splitext(s["file"])[0] + ".wav")

        # Step 1: Decode MP3 to 48kHz mono 16-bit PCM WAV using ffmpeg
        cmd = [
            "ffmpeg", "-y", "-i", mp3_path,
            "-ar", "48000", "-ac", "1",
            "-c:a", "pcm_s16le",
            raw_wav_path
        ]
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if res.returncode != 0:
            raise RuntimeError(f"FFmpeg failed on {mp3_path}: {res.stderr.decode('utf-8', errors='ignore')}")

        # Step 2: Read 16-bit PCM samples
        with open(raw_wav_path, "rb") as f:
            raw_bytes = f.read()

        # Simple WAV header parser: find 'data' chunk
        data_pos = raw_bytes.find(b'data')
        if data_pos == -1:
            raise RuntimeError(f"Could not find data chunk in {raw_wav_path}")
        data_size = struct.unpack('<I', raw_bytes[data_pos+4:data_pos+8])[0]
        pcm_data = raw_bytes[data_pos+8:data_pos+8+data_size]
        num_samples = len(pcm_data) // 2

        samples = [struct.unpack('<h', pcm_data[i*2:(i+1)*2])[0] / 32768.0 for i in range(num_samples)]

        # Trim leading MP3 encoder delay / pre-roll silence to guarantee instantaneous key strike response
        onset_idx = 0
        for i, val in enumerate(samples):
            if abs(val) >= 0.005:
                onset_idx = i
                break

        # Back up to preceding zero-crossing to eliminate clicks
        start_idx = max(0, onset_idx - 32)
        while start_idx > 0 and samples[start_idx] * samples[start_idx - 1] > 0:
            start_idx -= 1

        trimmed_samples = samples[start_idx:]
        fade_len = min(96, len(trimmed_samples))
        for fi in range(fade_len):
            trimmed_samples[fi] *= (fi / float(fade_len))
        samples = trimmed_samples

        # Peak normalization to -0.3 dBFS (~0.966)
        max_abs = max(abs(x) for x in samples) if samples else 1.0
        target_peak = 0.966
        scale = target_peak / max_abs if max_abs > 1e-6 else 1.0
        norm_samples = [max(-1.0, min(1.0, x * scale)) for x in samples]

        # Write normalized WAV
        norm_pcm = bytearray()
        for x in norm_samples:
            val = int(round(x * 32767.0))
            val = max(-32768, min(32767, val))
            norm_pcm.extend(struct.pack('<h', val))

        header = bytearray(raw_bytes[:data_pos+8])
        # Update RIFF size and data size
        riff_size = 36 + len(norm_pcm)
        header[4:8] = struct.pack('<I', riff_size)
        header[data_pos+4:data_pos+8] = struct.pack('<I', len(norm_pcm))

        with open(norm_wav_path, "wb") as f:
            f.write(header + norm_pcm)

        if os.path.exists(raw_wav_path):
            os.remove(raw_wav_path)

        duration = len(norm_samples) / 48000.0
        print(f"[{idx+1}/8] {s['name']}: {len(norm_samples)} samples ({duration:.3f} s), peak scaled by {scale:.2f}")

        processed_samples.append({
            "meta": s,
            "samples": norm_samples,
            "pcm16": norm_pcm
        })

    # =========================================================================
    # Step 3: Generate source/dsp/SampleBank.h
    # =========================================================================
    header_path = os.path.join(repo_root, "source", "dsp", "SampleBank.h")
    print(f"Generating C++ Header: {header_path}")
    with open(header_path, "w", encoding="utf-8") as f:
        f.write("""#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <string_view>

namespace ppf42 {

// ============================================================================
// SampleInfo: Static authentic audio asset metadata & PCM float data
// ============================================================================
struct SampleInfo {
    const char*        id;
    const char*        name;
    const char*        description;
    const float*       data;
    size_t             length;
    float              basePitchMidi; // Base root pitch: MIDI 48 (C3 = 130.81 Hz)
    double             sampleRate;    // 48000.0 Hz
};

inline constexpr size_t kNumEmbeddedSamples = 8;
inline constexpr float  kSampleBasePitchMidi = 48.0f; // C3
inline constexpr double kSampleBaseFreqHz   = 130.8127826502993;

class SampleBank {
public:
    static const SampleInfo& getSample(size_t index) noexcept;
    static constexpr size_t getSampleCount() noexcept { return kNumEmbeddedSamples; }
    static const std::array<SampleInfo, kNumEmbeddedSamples>& getAllSamples() noexcept;
};

} // namespace ppf42
""")

    # =========================================================================
    # Step 4: Generate source/dsp/SampleBank.cpp
    # =========================================================================
    cpp_path = os.path.join(repo_root, "source", "dsp", "SampleBank.cpp")
    print(f"Generating C++ Source: {cpp_path} (This embeds {sum(len(p['samples']) for p in processed_samples)} float samples)...")

    with open(cpp_path, "w", encoding="utf-8") as f:
        f.write("""#include "SampleBank.h"
#include <algorithm>

namespace ppf42 {

""")
        for idx, p in enumerate(processed_samples):
            f.write(f"// Sample {idx}: {p['meta']['name']} ({len(p['samples'])} samples)\n")
            f.write(f"static const float s_sample_{idx}_data[{len(p['samples'])}] = {{\n")
            samples = p['samples']
            # Format in rows of 10
            for i in range(0, len(samples), 10):
                row = samples[i:i+10]
                row_str = ", ".join(f"{x:.6f}f" for x in row)
                if i + 10 < len(samples):
                    row_str += ","
                f.write("    " + row_str + "\n")
            f.write("};\n\n")

        f.write("static const std::array<SampleInfo, kNumEmbeddedSamples> s_sampleBank = {{\n")
        for idx, p in enumerate(processed_samples):
            m = p['meta']
            f.write(f"""    SampleInfo {{
        "{m['id']}",
        "{m['name']}",
        "{m['desc']}",
        s_sample_{idx}_data,
        {len(p['samples'])},
        kSampleBasePitchMidi,
        48000.0
    }},\n""")
        f.write("}};\n\n")

        f.write("""const SampleInfo& SampleBank::getSample(size_t index) noexcept {
    if (index >= kNumEmbeddedSamples) index = 0;
    return s_sampleBank[index];
}

const std::array<SampleInfo, kNumEmbeddedSamples>& SampleBank::getAllSamples() noexcept {
    return s_sampleBank;
}

} // namespace ppf42
""")

    # =========================================================================
    # Step 5: Generate web/js/sample_bank.js
    # Base64-encoded 16-bit PCM for fast loading and zero-CORS browser playback
    # =========================================================================
    js_path = os.path.join(repo_root, "web", "js", "sample_bank.js")
    print(f"Generating Web JS Sample Bank: {js_path}...")

    with open(js_path, "w", encoding="utf-8") as f:
        f.write("""/**
 * PPF-42 DYNAMICS · Authentic Sample Bank
 * 8 high-fidelity authentic audio recordings embedded directly as Base64 16-bit PCM.
 * 100% zero-CORS, zero external file dependency, works offline and on Vercel.
 */

(function (global) {
    'use strict';

    function base64ToInt16Array(base64) {
        const binary = atob(base64);
        const len = binary.length;
        const bytes = new Uint8Array(len);
        for (let i = 0; i < len; i++) {
            bytes[i] = binary.charCodeAt(i);
        }
        return new Int16Array(bytes.buffer);
    }

    function int16ToFloat32Array(int16) {
        const len = int16.length;
        const float32 = new Float32Array(len);
        for (let i = 0; i < len; i++) {
            float32[i] = int16[i] / 32768.0;
        }
        return float32;
    }

    const SAMPLE_DEFINITIONS = [
""")
        for idx, p in enumerate(processed_samples):
            m = p['meta']
            b64_str = base64.b64encode(p['pcm16']).decode('ascii')
            f.write(f"""        {{
            index: {idx},
            id: "{m['id']}",
            name: "{m['name']}",
            description: "{m['desc']}",
            sampleRate: 48000,
            basePitchMidi: 48, // C3
            numSamples: {len(p['samples'])},
            base64Data: "{b64_str}"
        }},\n""")

        f.write("""    ];

    class SampleBank {
        constructor() {
            this.samples = SAMPLE_DEFINITIONS.map(def => ({
                ...def,
                floatData: null,
                audioBuffer: null
            }));
        }

        getSampleCount() {
            return this.samples.length;
        }

        getSample(index) {
            const idx = Math.max(0, Math.min(this.samples.length - 1, index || 0));
            return this.samples[idx];
        }

        getFloatData(index) {
            const sample = this.getSample(index);
            if (!sample.floatData) {
                const int16 = base64ToInt16Array(sample.base64Data);
                sample.floatData = int16ToFloat32Array(int16);
            }
            return sample.floatData;
        }

        getAudioBuffer(audioCtx, index) {
            if (!audioCtx) return null;
            const sample = this.getSample(index);
            if (!sample.audioBuffer) {
                const floatData = this.getFloatData(index);
                const buffer = audioCtx.createBuffer(1, floatData.length, sample.sampleRate);
                buffer.copyToChannel(floatData, 0);
                sample.audioBuffer = buffer;
            }
            return sample.audioBuffer;
        }

        preloadAll(audioCtx) {
            for (let i = 0; i < this.samples.length; i++) {
                this.getFloatData(i);
                if (audioCtx) {
                    this.getAudioBuffer(audioCtx, i);
                }
            }
        }
    }

    const sampleBankInstance = new SampleBank();
    global.SampleBank = sampleBankInstance;

})(typeof window !== 'undefined' ? window : this);
""")

    print("[SUCCESS] Audio Conversion, C++ Embedding, and Web JS Bank generation completed successfully!")

if __name__ == "__main__":
    main()
