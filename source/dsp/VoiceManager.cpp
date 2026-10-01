#include "VoiceManager.h"
#include <cmath>
#include <algorithm>

namespace ppf42 {

static inline float noteToFreq(float noteWithPitchBend) noexcept {
    const float raw = 440.0f * std::pow(2.0f, (noteWithPitchBend - 69.0f) / 12.0f);
    return std::clamp(raw, kMinOscFrequencyHz, kMaxOscFrequencyHz);
}

// ============================================================================
// PhysicalVoice Implementation
// ============================================================================
void PhysicalVoice::prepare(double sampleRate) noexcept {
    mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
    mOsc.prepare(sampleRate);
    mFluid.prepare(sampleRate);
    mEnvelope.prepare(sampleRate);
    reset();
}

void PhysicalVoice::reset() noexcept {
    mNoteNumber = -1;
    mVelocity = 0.0f;
    mPitchBendSemi = 0.0f;
    mAftertouch = 0.0f;
    mPedalLatched = false;
    mAgeSamples = 0;
    mCurrentFrequency = 110.0f;
    mTargetFrequency = 110.0f;
    mGlideCoeff = 1.0f;
    mDeclickGain = 1.0f;
    mDeclickDelta = 0.0f;
    mSubBassPhase = 0.0f;

    mOsc.reset();
    mFluid.reset();
    mEnvelope.reset();
}

void PhysicalVoice::noteOn(int noteNumber, float velocity, float initialGlideFreq) noexcept {
    const bool wasActive = isActive();
    mNoteNumber = noteNumber;
    mVelocity = std::clamp(velocity, 0.01f, 1.0f);
    mPedalLatched = false;
    mAgeSamples = 0;

    if (wasActive) {
        // 5 ms de-click crossfade (fade-in to prevent clicks on voice steal/retrigger)
        const float fadeSamples = std::max(16.0f, 0.005f * mSampleRate);
        mDeclickGain = 0.0f;
        mDeclickDelta = 1.0f / fadeSamples;
    } else {
        mDeclickGain = 1.0f;
        mDeclickDelta = 0.0f;
    }

    const float targetFreq = noteToFreq(static_cast<float>(noteNumber) + mPitchBendSemi);
    mTargetFrequency = targetFreq;

    if (initialGlideFreq > 1.0f) {
        mCurrentFrequency = initialGlideFreq;
    } else {
        mCurrentFrequency = targetFreq;
    }

    mEnvelope.noteOn(mVelocity);
}

void PhysicalVoice::noteOff() noexcept {
    if (!mPedalLatched) {
        mEnvelope.noteOff();
    }
}

void PhysicalVoice::setPitchBend(float semitones) noexcept {
    mPitchBendSemi = semitones;
    if (mNoteNumber >= 0) {
        mTargetFrequency = noteToFreq(static_cast<float>(mNoteNumber) + mPitchBendSemi);
    }
}

void PhysicalVoice::setAftertouch(float pressure) noexcept {
    mAftertouch = std::clamp(pressure, 0.0f, 1.0f);
}

void PhysicalVoice::startFadeOut() noexcept {
    // 5 ms de-click crossfade
    const float fadeSamples = std::max(16.0f, 0.005f * mSampleRate);
    mDeclickDelta = -1.0f / fadeSamples;
}

bool PhysicalVoice::isActive() const noexcept {
    return mEnvelope.isActive() || (mDeclickDelta < 0.0f && mDeclickGain > 1.0e-4f);
}

void PhysicalVoice::setFrequencyTarget(float targetHz, float glideTimeMs) noexcept {
    mTargetFrequency = std::clamp(targetHz, kMinOscFrequencyHz, kMaxOscFrequencyHz);
    if (glideTimeMs <= 0.5f) {
        mCurrentFrequency = mTargetFrequency;
        mGlideCoeff = 1.0f;
    } else {
        const float timeSec = glideTimeMs * 0.001f;
        mGlideCoeff = 1.0f - std::exp(-1.0f / (mSampleRate * timeSec * 0.35f));
    }
}

float PhysicalVoice::processSample(const ParameterSnapshot& params,
                                  float& outAirVelocity,
                                  float& outAperture,
                                  float& outBubbleActivity,
                                  float& outDropletPop) noexcept {
    if (!isActive()) {
        outAirVelocity = 0.0f;
        outAperture = 0.0f;
        outBubbleActivity = 0.0f;
        outDropletPop = 0.0f;
        return 0.0f;
    }

    ++mAgeSamples;

    // 1. Frequency glide slew
    mCurrentFrequency += mGlideCoeff * (mTargetFrequency - mCurrentFrequency);
    mCurrentFrequency = flushDenormal(mCurrentFrequency);

    // 2. Colonic pressure head from velocity, envelope, and aftertouch
    const float envLevel = mEnvelope.process(params.env_attack, params.env_decay, params.env_sustain, params.env_release);
    const float velScale = 0.30f + 0.70f * (mVelocity * mVelocity);
    const float aftertouchBoost = 1.0f + 0.50f * mAftertouch;
    const float effPressure = std::clamp(params.pressure * velScale * envLevel * aftertouchBoost, 0.0f, 1.0f);

    // 3. Sphincter valve physical oscillator
    SphincterOscParams oscP;
    oscP.pressure    = effPressure;
    oscP.tension     = params.tension;
    oscP.aperture    = params.aperture;
    oscP.flutter     = params.flutter;
    oscP.viscosity   = params.viscosity;
    oscP.frequencyHz = mCurrentFrequency;

    float airVel = 0.0f;
    float aperture = 0.0f;
    const float oscWave = mOsc.processSample(oscP, airVel, aperture);

    // 4. Multiphase fluid turbulence and bubbles
    FluidEngineParams fluidP;
    fluidP.viscosity   = params.viscosity;
    fluidP.moisture    = params.moisture;
    fluidP.dropletRate = params.droplet_rate;

    const float fluidWave = mFluid.processSample(airVel, aperture, fluidP);

    // 5. Phase-continuous Sub-bass sine generator
    const float phaseInc = mCurrentFrequency * (kTwoPi / mSampleRate);
    mSubBassPhase += phaseInc;
    if (mSubBassPhase >= kTwoPi) mSubBassPhase -= kTwoPi;

    const float subGain = dbToGain(params.sub_level);
    const float subBassWave = std::sin(mSubBassPhase) * subGain * envLevel;

    // 6. Voice summation
    float voiceOutput = oscWave + fluidWave + subBassWave;

    // 7. Apply de-click crossfading
    if (mDeclickDelta > 0.0f) {
        mDeclickGain += mDeclickDelta;
        if (mDeclickGain >= 1.0f) {
            mDeclickGain = 1.0f;
            mDeclickDelta = 0.0f;
        }
        voiceOutput *= mDeclickGain;
    } else if (mDeclickDelta < 0.0f) {
        mDeclickGain += mDeclickDelta;
        if (mDeclickGain <= 0.0f) {
            mDeclickGain = 0.0f;
            mEnvelope.reset();
        }
        voiceOutput *= mDeclickGain;
    }

    outAirVelocity = airVel;
    outAperture = aperture;
    outBubbleActivity = mFluid.getBubbleActivity();
    outDropletPop = mFluid.getDropletPopTrigger();

    return flushDenormal(voiceOutput);
}

// ============================================================================
// VoiceManager Implementation
// ============================================================================
void VoiceManager::prepare(double sampleRate) noexcept {
    mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
    for (auto& v : mVoices) {
        v.prepare(sampleRate);
    }
    reset();
}

void VoiceManager::reset() noexcept {
    for (auto& v : mVoices) {
        v.reset();
    }
    mNoteStackSize = 0;
    mLastVoiceMode = -1;
    mDamperPedalDown = false;
    mPitchBendSemi = 0.0f;
    mChannelPressure = 0.0f;
    mLastMonoNote = -1;
}

int VoiceManager::getActiveVoiceCount() const noexcept {
    int count = 0;
    for (const auto& v : mVoices) {
        if (v.isActive()) ++count;
    }
    return count;
}

int VoiceManager::findVoiceToSteal() noexcept {
    // Tier 1: Check for inactive voices
    for (size_t i = 0; i < kMaxPolyVoices; ++i) {
        if (!mVoices[i].isActive()) {
            return static_cast<int>(i);
        }
    }

    // Tier 2: Check for released voices (oldest first)
    int bestCandidate = -1;
    uint64_t oldestAge = 0;
    const uint64_t transientWindowSamples = static_cast<uint64_t>(0.060f * mSampleRate); // 60 ms protection

    for (size_t i = 0; i < kMaxPolyVoices; ++i) {
        if (mVoices[i].isReleased() && !mVoices[i].isLatched() && mVoices[i].getAge() > transientWindowSamples) {
            if (mVoices[i].getAge() > oldestAge) {
                oldestAge = mVoices[i].getAge();
                bestCandidate = static_cast<int>(i);
            }
        }
    }
    if (bestCandidate >= 0) return bestCandidate;

    // Fallback within Tier 2: any released voice
    for (size_t i = 0; i < kMaxPolyVoices; ++i) {
        if (mVoices[i].isReleased() && !mVoices[i].isLatched()) {
            if (mVoices[i].getAge() > oldestAge) {
                oldestAge = mVoices[i].getAge();
                bestCandidate = static_cast<int>(i);
            }
        }
    }
    if (bestCandidate >= 0) return bestCandidate;

    // Tier 3: Check pedal-latched voices
    oldestAge = 0;
    for (size_t i = 0; i < kMaxPolyVoices; ++i) {
        if (mVoices[i].isLatched() && mVoices[i].getAge() > transientWindowSamples) {
            if (mVoices[i].getAge() > oldestAge) {
                oldestAge = mVoices[i].getAge();
                bestCandidate = static_cast<int>(i);
            }
        }
    }
    if (bestCandidate >= 0) return bestCandidate;

    // Fallback within Tier 3: any pedal-latched voice
    for (size_t i = 0; i < kMaxPolyVoices; ++i) {
        if (mVoices[i].isLatched()) {
            if (mVoices[i].getAge() > oldestAge) {
                oldestAge = mVoices[i].getAge();
                bestCandidate = static_cast<int>(i);
            }
        }
    }
    if (bestCandidate >= 0) return bestCandidate;

    // Tier 4: Physically held voices (oldest first)
    oldestAge = 0;
    for (size_t i = 0; i < kMaxPolyVoices; ++i) {
        if (!mVoices[i].isReleased() && !mVoices[i].isLatched()) {
            if (mVoices[i].getAge() > oldestAge) {
                oldestAge = mVoices[i].getAge();
                bestCandidate = static_cast<int>(i);
            }
        }
    }
    if (bestCandidate >= 0) return bestCandidate;

    // Absolute fallback: oldest voice overall
    for (size_t i = 0; i < kMaxPolyVoices; ++i) {
        if (mVoices[i].getAge() > oldestAge) {
            oldestAge = mVoices[i].getAge();
            bestCandidate = static_cast<int>(i);
        }
    }

    return (bestCandidate >= 0) ? bestCandidate : 0;
}

void VoiceManager::handleNoteOn(int noteNumber, float velocity, const ParameterSnapshot& params) noexcept {
    if (params.voice_mode != mLastVoiceMode) {
        mNoteStackSize = 0;
        mLastVoiceMode = params.voice_mode;
    }

    if (params.voice_mode == 0) { // Mono Legato Mode
        // Push onto note stack
        if (mNoteStackSize < mNoteStack.size()) {
            mNoteStack[mNoteStackSize++] = { noteNumber, velocity };
        }

        const float targetFreq = noteToFreq(static_cast<float>(noteNumber) + mPitchBendSemi);

        if (!mVoices[0].isActive()) {
            // First note pressed: full retrigger
            mVoices[0].noteOn(noteNumber, velocity);
            mLastMonoNote = noteNumber;
        } else {
            // Legato sustain: smooth pitch glide without retriggering envelope
            mVoices[0].setFrequencyTarget(targetFreq, params.glide_time);
            mLastMonoNote = noteNumber;
        }
    }
    else if (params.voice_mode == 2) { // Stereo Unison Detune Mode
        // Unison stacks 4 voices with detune offsets: -7, +7, -14, +14 cents
        constexpr std::array<float, 4> detuneCents = { -7.0f, +7.0f, -14.0f, +14.0f };
        for (size_t i = 0; i < 4; ++i) {
            const float semiOffset = detuneCents[i] * 0.01f + mPitchBendSemi;
            const float freq = noteToFreq(static_cast<float>(noteNumber) + semiOffset);
            mVoices[i].noteOn(noteNumber, velocity);
            mVoices[i].setFrequencyTarget(freq, params.glide_time);
        }
    }
    else { // Polyphonic 8-Voice Mode
        // Find existing voice playing this note to retrigger
        int voiceIdx = -1;
        for (size_t i = 0; i < kMaxPolyVoices; ++i) {
            if (mVoices[i].isActive() && mVoices[i].getNoteNumber() == noteNumber) {
                voiceIdx = static_cast<int>(i);
                break;
            }
        }

        if (voiceIdx < 0) {
            voiceIdx = findVoiceToSteal();
        }

        mVoices[voiceIdx].noteOn(noteNumber, velocity);
        mVoices[voiceIdx].setPitchBend(mPitchBendSemi);
        mVoices[voiceIdx].setAftertouch(mChannelPressure);
    }
}

void VoiceManager::handleNoteOff(int noteNumber, const ParameterSnapshot& params) noexcept {
    if (params.voice_mode != mLastVoiceMode) {
        mNoteStackSize = 0;
        mLastVoiceMode = params.voice_mode;
    }

    if (mDamperPedalDown) {
        for (auto& v : mVoices) {
            if (v.isActive() && v.getNoteNumber() == noteNumber) {
                v.setLatched(true);
            }
        }
    }

    // Check Mono-Legato note stack only in Mono-Legato mode (mode 0)
    if (params.voice_mode == 0) {
        if (mNoteStackSize > 0) {
            size_t foundIdx = mNoteStackSize;
            for (size_t i = 0; i < mNoteStackSize; ++i) {
                if (mNoteStack[i].noteNumber == noteNumber) {
                    foundIdx = i;
                    break;
                }
            }
            if (foundIdx < mNoteStackSize) {
                // Remove from stack
                for (size_t i = foundIdx; i + 1 < mNoteStackSize; ++i) {
                    mNoteStack[i] = mNoteStack[i + 1];
                }
                --mNoteStackSize;
            }

            if (mNoteStackSize > 0) {
                // Glide back to top remaining note in stack
                const auto& top = mNoteStack[mNoteStackSize - 1];
                const float topFreq = noteToFreq(static_cast<float>(top.noteNumber) + mPitchBendSemi);
                mVoices[0].setFrequencyTarget(topFreq, params.glide_time > 0.0f ? params.glide_time : 50.0f);
                mLastMonoNote = top.noteNumber;
                return;
            }
        }

        // Mono note stack empty: release voice 0
        mVoices[0].noteOff();
        mLastMonoNote = -1;
        return;
    }

    // Polyphonic (mode 1) or Stereo Unison (mode 2):
    // Release matching active voices
    for (auto& v : mVoices) {
        if (v.isActive() && v.getNoteNumber() == noteNumber) {
            v.noteOff();
        }
    }
}

void VoiceManager::handlePitchBend(int /*channel*/, int msb, int lsb) noexcept {
    // 14-bit pitch bend: center is 8192 (0 semitones), range +-2 semitones
    const int val14 = (msb << 7) | lsb;
    mPitchBendSemi = static_cast<float>(val14 - 8192) * (2.0f / 8192.0f);

    for (auto& v : mVoices) {
        if (v.isActive()) {
            v.setPitchBend(mPitchBendSemi);
        }
    }
}

void VoiceManager::handleChannelPressure(int /*channel*/, int pressure) noexcept {
    mChannelPressure = static_cast<float>(pressure) * (1.0f / 127.0f);
    for (auto& v : mVoices) {
        if (v.isActive()) {
            v.setAftertouch(mChannelPressure);
        }
    }
}

void VoiceManager::handlePolyAftertouch(int noteNumber, int pressure) noexcept {
    const float p = static_cast<float>(pressure) * (1.0f / 127.0f);
    for (auto& v : mVoices) {
        if (v.isActive() && v.getNoteNumber() == noteNumber) {
            v.setAftertouch(p);
        }
    }
}

void VoiceManager::handleControlChange(int cc, int val) noexcept {
    if (cc == 64) { // Damper / Sustain Pedal
        mDamperPedalDown = (val >= 64);
        if (!mDamperPedalDown) {
            for (auto& v : mVoices) {
                if (v.isLatched()) {
                    v.setLatched(false);
                    v.noteOff();
                }
            }
        }
    } else if (cc == 123 || cc == 120) { // All Notes Off / All Sound Off
        allNotesOff();
    }
}

void VoiceManager::allNotesOff() noexcept {
    mNoteStackSize = 0;
    mDamperPedalDown = false;
    for (auto& v : mVoices) {
        v.noteOff();
    }
}

void VoiceManager::handleMidiEvent(const MidiEvent& event, const ParameterSnapshot& params) noexcept {
    const uint8_t statusType = event.status & 0xF0;
    const uint8_t channel    = event.status & 0x0F;

    switch (statusType) {
        case 0x90: { // Note On
            const int note = event.data1;
            const float vel = static_cast<float>(event.data2) * (1.0f / 127.0f);
            if (event.data2 > 0) {
                handleNoteOn(note, vel, params);
            } else {
                handleNoteOff(note, params);
            }
            break;
        }

        case 0x80: { // Note Off
            handleNoteOff(event.data1, params);
            break;
        }

        case 0xE0: { // Pitch Bend
            handlePitchBend(channel, event.data2, event.data1);
            break;
        }

        case 0xD0: { // Channel Pressure (Aftertouch)
            handleChannelPressure(channel, event.data1);
            break;
        }

        case 0xA0: { // Polyphonic Key Pressure
            handlePolyAftertouch(event.data1, event.data2);
            break;
        }

        case 0xB0: { // Control Change
            handleControlChange(event.data1, event.data2);
            break;
        }

        default:
            break;
    }
}

void VoiceManager::processSample(const ParameterSnapshot& params,
                                 float& outL, float& outR,
                                 float& outAirVelocity,
                                 float& outAperture,
                                 float& outBubbleActivity,
                                 float& outDropletPop) noexcept {
    outL = 0.0f;
    outR = 0.0f;
    outAirVelocity = 0.0f;
    outAperture = 0.0f;
    outBubbleActivity = 0.0f;
    outDropletPop = 0.0f;

    if (params.voice_mode != mLastVoiceMode) {
        mNoteStackSize = 0;
        mLastVoiceMode = params.voice_mode;
    }

    if (params.voice_mode == 0) { // Mono Legato Mode
        float airVel = 0.0f, ap = 0.0f, bubbles = 0.0f, pop = 0.0f;
        const float s = mVoices[0].processSample(params, airVel, ap, bubbles, pop);
        outL = s;
        outR = s;
        outAirVelocity = airVel;
        outAperture = ap;
        outBubbleActivity = bubbles;
        outDropletPop = pop;
    }
    else if (params.voice_mode == 2) { // Stereo Unison Detune Mode
        // 4 stereo voices:
        // V0: L 0.85, R 0.15
        // V1: L 0.15, R 0.85
        // V2: L 0.65, R 0.35
        // V3: L 0.35, R 0.65
        constexpr std::array<float, 4> panL = { 0.85f, 0.15f, 0.65f, 0.35f };
        constexpr std::array<float, 4> panR = { 0.15f, 0.85f, 0.35f, 0.65f };

        float maxAirVel = 0.0f;
        float maxAp = 0.0f;
        float sumBubbles = 0.0f;
        float maxPop = 0.0f;

        for (size_t i = 0; i < 4; ++i) {
            float airVel = 0.0f, ap = 0.0f, bubbles = 0.0f, pop = 0.0f;
            const float s = mVoices[i].processSample(params, airVel, ap, bubbles, pop);

            outL += s * panL[i];
            outR += s * panR[i];

            maxAirVel = std::max(maxAirVel, airVel);
            maxAp = std::max(maxAp, ap);
            sumBubbles += bubbles;
            maxPop = std::max(maxPop, pop);
        }

        // Normalize by 1 / sqrt(N_voices) = 0.5
        outL *= 0.50f;
        outR *= 0.50f;

        outAirVelocity = maxAirVel;
        outAperture = maxAp;
        outBubbleActivity = std::clamp(sumBubbles * 0.25f, 0.0f, 1.0f);
        outDropletPop = maxPop;
    }
    else { // Polyphonic 8-Voice Mode
        float sumL = 0.0f;
        float sumR = 0.0f;
        float maxAirVel = 0.0f;
        float maxAp = 0.0f;
        float sumBubbles = 0.0f;
        float maxPop = 0.0f;
        int activeCount = 0;

        for (size_t i = 0; i < kMaxPolyVoices; ++i) {
            if (!mVoices[i].isActive()) continue;

            ++activeCount;
            float airVel = 0.0f, ap = 0.0f, bubbles = 0.0f, pop = 0.0f;
            const float s = mVoices[i].processSample(params, airVel, ap, bubbles, pop);

            // Subtle stereo alternation across polyphonic voices
            const float pan = 0.40f + 0.20f * (static_cast<float>(i % 3));
            sumL += s * pan;
            sumR += s * (1.0f - pan);

            maxAirVel = std::max(maxAirVel, airVel);
            maxAp = std::max(maxAp, ap);
            sumBubbles += bubbles;
            maxPop = std::max(maxPop, pop);
        }

        if (activeCount > 1) {
            // Polyphonic voice scaling factor
            const float norm = 1.0f / std::sqrt(static_cast<float>(activeCount));
            sumL *= norm;
            sumR *= norm;
        }

        outL = sumL;
        outR = sumR;
        outAirVelocity = maxAirVel;
        outAperture = maxAp;
        outBubbleActivity = std::clamp(sumBubbles * 0.25f, 0.0f, 1.0f);
        outDropletPop = maxPop;
    }

    outL = flushDenormal(outL);
    outR = flushDenormal(outR);
}

} // namespace ppf42
