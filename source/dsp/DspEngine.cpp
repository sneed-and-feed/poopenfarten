#include "DspEngine.h"
#include <cmath>
#include <algorithm>

namespace ppf42 {

void DspEngine::prepare(double sampleRate, int maxBlockSize) noexcept {
    mSampleRate = sampleRate > 100.0 ? sampleRate : kDefaultSampleRate;
    mMaxBlockSize = maxBlockSize > 0 ? maxBlockSize : kDefaultBlockSize;

    mVoiceManager.prepare(mSampleRate);
    mWaveguideL.prepare(mSampleRate);
    mWaveguideR.prepare(mSampleRate);
    mConvolverL.prepare(mSampleRate);
    mConvolverR.prepare(mSampleRate);
    mDcBlockerL.prepare(mSampleRate);
    mDcBlockerR.prepare(mSampleRate);

    mTelemetryIntervalSamples = std::max(64, static_cast<int>(mSampleRate / 60.0));
    reset();
}

void DspEngine::reset() noexcept {
    mVoiceManager.reset();
    mWaveguideL.reset();
    mWaveguideR.reset();
    mConvolverL.reset();
    mConvolverR.reset();
    mDcBlockerL.reset();
    mDcBlockerR.reset();
    mTelemetry.reset();

    mTelemetrySampleCounter = 0;
    mCurrentFrame = VisualizerFrame {};
    mScopeWriteIdx = 0;
    mRmsAccumL = 0.0f;
    mRmsAccumR = 0.0f;
}

void DspEngine::process(float* left, float* right, int numSamples,
                        const ParameterSnapshot& params,
                        const MidiEvent* midiEvents, int numMidiEvents) noexcept {
    // Hardware FTZ/DAZ mode enforcement
    ScopedNoDenormals noDenormals;

    // Apply coupled performance macros (Gut Squeeze and Dietary Moisture)
    const ParameterSnapshot effParams = applyMacroCoupling(params);
    const float masterGainLinear = dbToGain(effParams.master_gain);

    int nextMidiIdx = 0;

    for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx) {
        // Dispatch MIDI events happening at or before this sample
        while (nextMidiIdx < numMidiEvents && midiEvents[nextMidiIdx].sampleOffset <= sampleIdx) {
            mVoiceManager.handleMidiEvent(midiEvents[nextMidiIdx], effParams);
            ++nextMidiIdx;
        }

        // 1. Voice Manager: Exciter + Multiphase Fluid + Sub-Bass
        float vL = 0.0f, vR = 0.0f;
        float airVel = 0.0f, aperture = 0.0f, bubbleAct = 0.0f, dropletPop = 0.0f;
        mVoiceManager.processSample(effParams, vL, vR, airVel, aperture, bubbleAct, dropletPop);

        // 2. Intergluteal Cleft Waveguide
        const float wgL = mWaveguideL.processSample(vL, effParams.cleft_damping);
        const float wgR = mWaveguideR.processSample(vR, effParams.cleft_damping);

        // 3. Ceramic Porcelain Cavity Convolver
        const float convL = mConvolverL.processSample(wgL, effParams.porcelain_mix, effParams.porcelain_size, effParams.porcelain_model);
        const float convR = mConvolverR.processSample(wgR, effParams.porcelain_mix, effParams.porcelain_size, effParams.porcelain_model);

        // 4. Infrasonic 15 Hz DC Blocker
        const float dcL = mDcBlockerL.processSample(convL);
        const float dcR = mDcBlockerR.processSample(convR);

        // 5. Harmonic Saturation Drive Stage
        const float satL = BoundedSaturator::processDrive(dcL, effParams.drive);
        const float satR = BoundedSaturator::processDrive(dcR, effParams.drive);

        // 6. Master True-Peak Brickwall Limiter (guaranteed <= 0.0 dBFS)
        const float outL = BoundedSaturator::limitSample(satL, masterGainLinear);
        const float outR = BoundedSaturator::limitSample(satR, masterGainLinear);

        if (left)  left[sampleIdx]  = outL;
        if (right) right[sampleIdx] = outR;

        // 7. Decimated Telemetry Accumulation
        if (mScopeWriteIdx < kTelemetryScopeSamples) {
            // Decimate audio to fit 128 scope samples per 60 Hz frame
            const int decimationFactor = std::max(1, mTelemetryIntervalSamples / static_cast<int>(kTelemetryScopeSamples));
            if ((sampleIdx % decimationFactor) == 0) {
                mCurrentFrame.scopeSamples[mScopeWriteIdx++] = (outL + outR) * 0.5f;
            }
        }

        mRmsAccumL += outL * outL;
        mRmsAccumR += outR * outR;

        mCurrentFrame.instantaneousAperture = aperture;
        mCurrentFrame.airflowVelocity = airVel;
        mCurrentFrame.colonicPressure = effParams.pressure;
        mCurrentFrame.bubbleActivity = bubbleAct;
        if (dropletPop > mCurrentFrame.dropletPops) {
            mCurrentFrame.dropletPops = dropletPop;
        }
        mCurrentFrame.cleftResonanceEnergy = (mWaveguideL.getResonanceEnergy() + mWaveguideR.getResonanceEnergy()) * 0.5f;

        if (++mTelemetrySampleCounter >= mTelemetryIntervalSamples) {
            mTelemetrySampleCounter = 0;
            const float invCount = 1.0f / static_cast<float>(mTelemetryIntervalSamples);
            mCurrentFrame.outputRmsL = std::sqrt(flushDenormal(mRmsAccumL * invCount));
            mCurrentFrame.outputRmsR = std::sqrt(flushDenormal(mRmsAccumR * invCount));

            // Pad remainder of scope buffer if needed
            while (mScopeWriteIdx < kTelemetryScopeSamples) {
                mCurrentFrame.scopeSamples[mScopeWriteIdx++] = (outL + outR) * 0.5f;
            }

            mTelemetry.push(mCurrentFrame);

            // Reset frame accumulators for next interval
            mScopeWriteIdx = 0;
            mRmsAccumL = 0.0f;
            mRmsAccumR = 0.0f;
            mCurrentFrame.dropletPops = 0.0f;
        }
    }

    // Process any remaining MIDI events at end of block
    while (nextMidiIdx < numMidiEvents) {
        mVoiceManager.handleMidiEvent(midiEvents[nextMidiIdx++], effParams);
    }
}

} // namespace ppf42
