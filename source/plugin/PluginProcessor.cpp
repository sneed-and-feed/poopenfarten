#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ppf42 {

PPF42AudioProcessor::PPF42AudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

PPF42AudioProcessor::~PPF42AudioProcessor()
{
}

const juce::String PPF42AudioProcessor::getName() const
{
    return "PPF-42 DYNAMICS";
}

bool PPF42AudioProcessor::acceptsMidi() const
{
    return true;
}

bool PPF42AudioProcessor::producesMidi() const
{
    return false;
}

bool PPF42AudioProcessor::isMidiEffect() const
{
    return false;
}

double PPF42AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PPF42AudioProcessor::getNumPrograms()
{
    return getNumPresets();
}

int PPF42AudioProcessor::getCurrentProgram()
{
    return currentProgramIndex.load(std::memory_order_relaxed);
}

void PPF42AudioProcessor::setCurrentProgram(int index)
{
    const int idx = std::clamp(index, 0, getNumPresets() - 1);
    currentProgramIndex.store(idx, std::memory_order_relaxed);
    applyPreset(idx, apvts);
}

const juce::String PPF42AudioProcessor::getProgramName(int index)
{
    return getPresetName(index);
}

void PPF42AudioProcessor::changeProgramName(int, const juce::String&)
{
}

void PPF42AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    dspEngine.prepare(sampleRate, samplesPerBlock);
}

void PPF42AudioProcessor::releaseResources()
{
    dspEngine.reset();
}

bool PPF42AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void PPF42AudioProcessor::pushUIMidiRaw(uint8_t status, uint8_t d1, uint8_t d2) noexcept
{
    const int write = uiMidiWritePos.load(std::memory_order_relaxed);
    const int nextWrite = (write + 1) % kUIMidiQueueSize;
    if (nextWrite != uiMidiReadPos.load(std::memory_order_acquire))
    {
        uiMidiQueue[write] = { status, d1, d2 };
        uiMidiWritePos.store(nextWrite, std::memory_order_release);
    }
}

void PPF42AudioProcessor::pushUINoteOn(int noteNumber, float velocity) noexcept
{
    const uint8_t note = static_cast<uint8_t>(std::clamp(noteNumber, 0, 127));
    const uint8_t vel = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(velocity * 127.0f)), 1, 127));
    pushUIMidiRaw(0x90, note, vel);
}

void PPF42AudioProcessor::pushUINoteOff(int noteNumber, float velocity) noexcept
{
    const uint8_t note = static_cast<uint8_t>(std::clamp(noteNumber, 0, 127));
    const uint8_t vel = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(velocity * 127.0f)), 0, 127));
    pushUIMidiRaw(0x80, note, vel);
}

void PPF42AudioProcessor::pushUIAllNotesOff() noexcept
{
    pushUIMidiRaw(0xB0, 123, 0); // All Notes Off CC 123
    pushUIMidiRaw(0xB0, 120, 0); // All Sound Off CC 120
}

void PPF42AudioProcessor::pushUIPitchBend(float pitchBendCents) noexcept
{
    const float norm = std::clamp((pitchBendCents / 200.0f) * 8192.0f + 8192.0f, 0.0f, 16383.0f);
    const int bend14 = static_cast<int>(std::round(norm));
    const uint8_t lsb = static_cast<uint8_t>(bend14 & 0x7F);
    const uint8_t msb = static_cast<uint8_t>((bend14 >> 7) & 0x7F);
    pushUIMidiRaw(0xE0, lsb, msb);
}

void PPF42AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || buffer.getNumChannels() <= 0)
        return;

    // Unconditionally clear output buffers before synthesis
    buffer.clear();

    // Convert UI MIDI FIFO events + host DAW MIDI messages into stack array
    constexpr int kMaxMidiStack = 256;
    MidiEvent midiEventsStack[kMaxMidiStack];
    int eventCount = 0;

    // 1. Drain UI MIDI events (virtual keyboard, UI performance gestures)
    int uiRead = uiMidiReadPos.load(std::memory_order_relaxed);
    const int uiWrite = uiMidiWritePos.load(std::memory_order_acquire);
    while (uiRead != uiWrite && eventCount < kMaxMidiStack)
    {
        midiEventsStack[eventCount].sampleOffset = 0;
        midiEventsStack[eventCount].status = uiMidiQueue[uiRead].status;
        midiEventsStack[eventCount].data1  = uiMidiQueue[uiRead].data1;
        midiEventsStack[eventCount].data2  = uiMidiQueue[uiRead].data2;
        ++eventCount;
        uiRead = (uiRead + 1) % kUIMidiQueueSize;
    }
    uiMidiReadPos.store(uiRead, std::memory_order_release);

    // 2. Append host DAW MIDI messages
    for (const auto metadata : midiMessages)
    {
        if (eventCount >= kMaxMidiStack)
            break;

        const auto* rawData = metadata.data;
        const int numBytes = metadata.numBytes;
        if (numBytes >= 1)
        {
            midiEventsStack[eventCount].sampleOffset = metadata.samplePosition;
            midiEventsStack[eventCount].status = rawData[0];
            midiEventsStack[eventCount].data1  = (numBytes > 1) ? rawData[1] : 0;
            midiEventsStack[eventCount].data2  = (numBytes > 2) ? rawData[2] : 0;
            ++eventCount;
        }
    }
    midiMessages.clear();

    // Read atomic APVTS values into POD snapshot
    const ParameterSnapshot snapshot = createSnapshotFromAPVTS(apvts);

    float* left = buffer.getWritePointer(0);
    float* right = (buffer.getNumChannels() > 1) ? buffer.getWritePointer(1) : left;

    dspEngine.process(left, right, numSamples, snapshot, midiEventsStack, eventCount);
}

bool PPF42AudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* PPF42AudioProcessor::createEditor()
{
    return new PPF42AudioProcessorEditor(*this);
}

void PPF42AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("currentProgramIndex", currentProgramIndex.load(std::memory_order_relaxed), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PPF42AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        auto vt = juce::ValueTree::fromXml(*xmlState);
        apvts.replaceState(vt);
        if (vt.hasProperty("currentProgramIndex"))
        {
            currentProgramIndex.store(static_cast<int>(vt.getProperty("currentProgramIndex")), std::memory_order_relaxed);
        }
    }
}

} // namespace ppf42

// JUCE Plugin Filter Factory
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ppf42::PPF42AudioProcessor();
}
