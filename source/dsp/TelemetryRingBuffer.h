#pragma once

#include "DspDefines.h"
#include <atomic>
#include <array>
#include <cstddef>
#include <cstring>

namespace ppf42 {

// ============================================================================
// VisualizerFrame: Real-Time Telemetry Payload for 60 FPS Canvas Visualizers
// ============================================================================
struct VisualizerFrame {
    float instantaneousAperture { 0.0f }; // y(t) tissue opening
    float airflowVelocity       { 0.0f }; // v(t) air velocity through gap
    float colonicPressure       { 0.0f }; // Upstream pressure head
    float bubbleActivity        { 0.0f }; // Minnaert bubble density [0, 1]
    float dropletPops           { 0.0f }; // Stochastic droplet trigger impulse
    float cleftResonanceEnergy  { 0.0f }; // Waveguide energy
    float outputRmsL            { 0.0f }; // Left channel RMS
    float outputRmsR            { 0.0f }; // Right channel RMS
    float scopeSamples[kTelemetryScopeSamples] {}; // Decimated raw relaxation waveform
};

// ============================================================================
// TelemetryRingBuffer: Lock-Free Single-Producer Single-Consumer (SPSC) FIFO
// Guarantees strictly non-blocking audio execution and zero heap allocations.
// ============================================================================
class TelemetryRingBuffer {
public:
    static constexpr size_t kCapacity = kTelemetryQueueCapacity; // 32
    static constexpr size_t kMask     = kCapacity - 1;

    TelemetryRingBuffer() noexcept {
        reset();
    }

    void reset() noexcept {
        mWriteIdx.store(0, std::memory_order_relaxed);
        mReadIdx.store(0, std::memory_order_relaxed);
    }

    // Producer (Audio Thread): strictly non-blocking push
    bool push(const VisualizerFrame& frame) noexcept {
        const size_t w = mWriteIdx.load(std::memory_order_relaxed);
        const size_t r = mReadIdx.load(std::memory_order_acquire);

        // Check if queue is full
        if (((w + 1) & kMask) == (r & kMask)) {
            // Buffer full: drop frame to ensure audio thread never blocks
            return false;
        }

        mBuffer[w & kMask] = frame;
        mWriteIdx.store(w + 1, std::memory_order_release);
        return true;
    }

    // Consumer (UI / Timer Thread): pop next available frame
    bool pop(VisualizerFrame& frame) noexcept {
        const size_t r = mReadIdx.load(std::memory_order_relaxed);
        const size_t w = mWriteIdx.load(std::memory_order_acquire);

        if (r == w) {
            // Buffer empty
            return false;
        }

        frame = mBuffer[r & kMask];
        mReadIdx.store(r + 1, std::memory_order_release);
        return true;
    }

private:
    std::array<VisualizerFrame, kCapacity> mBuffer {};
    alignas(64) std::atomic<size_t> mWriteIdx { 0 };
    alignas(64) std::atomic<size_t> mReadIdx  { 0 };
};

} // namespace ppf42
