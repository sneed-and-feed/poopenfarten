#pragma once

#include "DspDefines.h"
#include "DspMath.h"
#include "MidiEvent.h"
#include "ParameterSnapshot.h"
#include "VoiceManager.h"
#include "CleftWaveguide.h"
#include "PorcelainConvolver.h"
#include "SubBassDcBlocker.h"
#include "BoundedSaturator.h"
#include "TelemetryRingBuffer.h"

namespace ppf42 {

// ============================================================================
// DspEngine: Top-Level Pure C++20 Aeroacoustic Synthesis Engine
// Operates with zero heap allocations, zero locks, and full re-entrancy.
// ============================================================================
class DspEngine {
public:
    DspEngine() noexcept = default;

    void prepare(double sampleRate, int maxBlockSize = kDefaultBlockSize) noexcept;
    void reset() noexcept;

    // Real-Time Audio Block Processing Loop
    void process(float* left, float* right, int numSamples,
                 const ParameterSnapshot& params,
                 const MidiEvent* midiEvents, int numMidiEvents) noexcept;

    // Pop telemetry frame for 60 FPS GUI visualizers
    bool popVisualizerFrame(VisualizerFrame& frame) noexcept {
        return mTelemetry.pop(frame);
    }

    [[nodiscard]] double getSampleRate() const noexcept { return mSampleRate; }
    [[nodiscard]] int getActiveVoiceCount() const noexcept { return mVoiceManager.getActiveVoiceCount(); }

private:
    double mSampleRate { kDefaultSampleRate };
    int    mMaxBlockSize { kDefaultBlockSize };

    VoiceManager       mVoiceManager;
    CleftWaveguide     mWaveguideL;
    CleftWaveguide     mWaveguideR;
    PorcelainConvolver mConvolverL;
    PorcelainConvolver mConvolverR;
    SubBassDcBlocker   mDcBlockerL;
    SubBassDcBlocker   mDcBlockerR;
    TelemetryRingBuffer mTelemetry;

    // Visualizer telemetry decimation counters
    int   mTelemetrySampleCounter { 0 };
    int   mTelemetryIntervalSamples { 800 }; // ~60 Hz at 48 kHz
    VisualizerFrame mCurrentFrame {};
    size_t mScopeWriteIdx { 0 };
    float mRmsAccumL { 0.0f };
    float mRmsAccumR { 0.0f };
};

} // namespace ppf42
