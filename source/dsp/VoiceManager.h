#pragma once

#include "DspDefines.h"
#include "DspMath.h"
#include "MidiEvent.h"
#include "ParameterSnapshot.h"
#include "SphincterOscillator.h"
#include "FluidNoiseEngine.h"

#include <array>
#include <cstdint>

namespace ppf42 {

// ============================================================================
// Pressure ADSR Envelope Generator
// ============================================================================
class PressureEnvelope {
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void prepare(double sampleRate) noexcept {
        mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
        reset();
    }

    void reset() noexcept {
        mStage = Stage::Idle;
        mCurrentLevel = 0.0f;
    }

    void noteOn(float velocity) noexcept {
        mVelocity = std::clamp(velocity, 0.01f, 1.0f);
        mStage = Stage::Attack;
    }

    void noteOff() noexcept {
        if (mStage != Stage::Idle) {
            mStage = Stage::Release;
        }
    }

    [[nodiscard]] bool isActive() const noexcept {
        return mStage != Stage::Idle;
    }

    [[nodiscard]] Stage getStage() const noexcept {
        return mStage;
    }

    [[nodiscard]] float getLevel() const noexcept {
        return mCurrentLevel;
    }

    [[nodiscard]] float process(float attackMs, float decayMs, float sustain, float releaseMs) noexcept {
        const float dt = 1.0f / mSampleRate;
        switch (mStage) {
            case Stage::Idle:
                mCurrentLevel = 0.0f;
                break;

            case Stage::Attack: {
                const float aTime = std::max(0.0001f, attackMs * 0.001f);
                mCurrentLevel += dt / aTime;
                if (mCurrentLevel >= 1.0f) {
                    mCurrentLevel = 1.0f;
                    mStage = Stage::Decay;
                }
                break;
            }

            case Stage::Decay: {
                const float dTime = std::max(0.001f, decayMs * 0.001f);
                const float target = std::clamp(sustain, 0.0f, 1.0f);
                const float rate = 1.0f - std::exp(-dt / (dTime * 0.35f));
                mCurrentLevel += rate * (target - mCurrentLevel);
                if (std::abs(mCurrentLevel - target) < 0.005f) {
                    mCurrentLevel = target;
                    mStage = Stage::Sustain;
                }
                break;
            }

            case Stage::Sustain:
                mCurrentLevel = std::clamp(sustain, 0.0f, 1.0f);
                break;

            case Stage::Release: {
                const float rTime = std::max(0.001f, releaseMs * 0.001f);
                const float rate = 1.0f - std::exp(-dt / (rTime * 0.35f));
                mCurrentLevel -= rate * mCurrentLevel;
                if (mCurrentLevel < 1.0e-4f) {
                    mCurrentLevel = 0.0f;
                    mStage = Stage::Idle;
                }
                break;
            }
        }
        mCurrentLevel = flushDenormal(mCurrentLevel);
        return mCurrentLevel;
    }

private:
    float mSampleRate   { 48000.0f };
    float mCurrentLevel { 0.0f };
    float mVelocity     { 1.0f };
    Stage mStage        { Stage::Idle };
};

// ============================================================================
// Physical Modeling Voice
// ============================================================================
class PhysicalVoice {
public:
    PhysicalVoice() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void noteOn(int noteNumber, float velocity, float initialGlideFreq = 0.0f) noexcept;
    void noteOff() noexcept;
    void setPitchBend(float semitones) noexcept;
    void setAftertouch(float pressure) noexcept;
    void startFadeOut() noexcept;

    [[nodiscard]] bool isActive() const noexcept;
    [[nodiscard]] bool isReleased() const noexcept {
        return mEnvelope.getStage() == PressureEnvelope::Stage::Release;
    }
    [[nodiscard]] int  getNoteNumber() const noexcept { return mNoteNumber; }
    [[nodiscard]] uint64_t getAge() const noexcept { return mAgeSamples; }
    [[nodiscard]] bool isLatched() const noexcept { return mPedalLatched; }
    void setLatched(bool latched) noexcept { mPedalLatched = latched; }

    [[nodiscard]] float processSample(const ParameterSnapshot& params,
                                     float& outAirVelocity,
                                     float& outAperture,
                                     float& outBubbleActivity,
                                     float& outDropletPop) noexcept;

    void setFrequencyTarget(float targetHz, float glideTimeMs) noexcept;

private:
    float mSampleRate        { 48000.0f };
    int   mNoteNumber        { -1 };
    float mVelocity          { 0.0f };
    float mPitchBendSemi     { 0.0f };
    float mAftertouch        { 0.0f };
    bool  mPedalLatched      { false };
    uint64_t mAgeSamples     { 0 };

    // Slew-rate pitch tracker for portamento glide
    float mCurrentFrequency  { 110.0f };
    float mTargetFrequency   { 110.0f };
    float mGlideCoeff        { 1.0f };

    // De-click 5 ms crossfade multiplier
    float mDeclickGain       { 1.0f };
    float mDeclickDelta      { 0.0f };

    // Dynamic pressure droop tracking
    float mPressureVented    { 0.0f };

    // Dynamic 2-pole resonant wet squelch formant filter (650 - 1200 Hz, Q ~ 4.5 - 6.0)
    BiquadDirectForm2T mSquelchFilter;

    // Asymmetric aerodynamic sub-bass displacement pulse lowpass filter
    OnePoleLowpass mSubThumpFilter;

    SphincterOscillator mOsc;
    FluidNoiseEngine    mFluid;
    PressureEnvelope    mEnvelope;
};

// ============================================================================
// VoiceManager: Mono-Legato, Poly 8-Voice, and Stereo Unison
// ============================================================================
class VoiceManager {
public:
    VoiceManager() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void handleMidiEvent(const MidiEvent& event, const ParameterSnapshot& params) noexcept;
    void allNotesOff() noexcept;

    void processSample(const ParameterSnapshot& params,
                       float& outL, float& outR,
                       float& outAirVelocity,
                       float& outAperture,
                       float& outBubbleActivity,
                       float& outDropletPop) noexcept;

    [[nodiscard]] int getActiveVoiceCount() const noexcept;

private:
    struct HeldNote {
        int noteNumber;
        float velocity;
    };

    void handleNoteOn(int noteNumber, float velocity, const ParameterSnapshot& params) noexcept;
    void handleNoteOff(int noteNumber, const ParameterSnapshot& params) noexcept;
    void handlePitchBend(int channel, int msb, int lsb) noexcept;
    void handleChannelPressure(int channel, int pressure) noexcept;
    void handlePolyAftertouch(int noteNumber, int pressure) noexcept;
    void handleControlChange(int cc, int val) noexcept;

    int findVoiceToSteal() noexcept;

    float mSampleRate { 48000.0f };
    std::array<PhysicalVoice, kMaxPolyVoices> mVoices {};

    // Mono-Legato Note Stack (up to 32 entries)
    std::array<HeldNote, 32> mNoteStack {};
    size_t mNoteStackSize { 0 };

    int   mLastVoiceMode   { -1 };
    bool  mDamperPedalDown { false };
    float mPitchBendSemi   { 0.0f };
    float mChannelPressure { 0.0f };
    int   mLastMonoNote    { -1 };
};

} // namespace ppf42
