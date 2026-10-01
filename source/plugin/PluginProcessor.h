#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/DspEngine.h"
#include "Parameters.h"
#include "Presets.h"
#include <atomic>

namespace ppf42 {

class PPF42AudioProcessor : public juce::AudioProcessor
{
public:
    PPF42AudioProcessor();
    ~PPF42AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    DspEngine& getDspEngine() noexcept { return dspEngine; }

    // Lock-Free UI MIDI Event Injection (Virtual Keyboard, UI Pads, Sequencer)
    void pushUINoteOn(int noteNumber, float velocity) noexcept;
    void pushUINoteOff(int noteNumber, float velocity = 0.0f) noexcept;
    void pushUIAllNotesOff() noexcept;
    void pushUIPitchBend(float pitchBendCents) noexcept;
    void pushUIMidiRaw(uint8_t status, uint8_t d1, uint8_t d2) noexcept;

    // Pop telemetry frame for 60 FPS Visualizer streaming
    bool popVisualizerFrame(VisualizerFrame& frame) noexcept {
        return dspEngine.popVisualizerFrame(frame);
    }

private:
    juce::AudioProcessorValueTreeState apvts;
    DspEngine dspEngine;

    std::atomic<int> currentProgramIndex { 0 };

    // Lock-free FIFO queue for UI MIDI events
    struct UIMidiEvent {
        uint8_t status { 0 };
        uint8_t data1  { 0 };
        uint8_t data2  { 0 };
    };
    static constexpr int kUIMidiQueueSize = 256;
    UIMidiEvent uiMidiQueue[kUIMidiQueueSize] {};
    std::atomic<int> uiMidiWritePos { 0 };
    std::atomic<int> uiMidiReadPos  { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PPF42AudioProcessor)
};

} // namespace ppf42
