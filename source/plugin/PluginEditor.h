#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "Parameters.h"
#include "Presets.h"
#if JUCE_WEB_BROWSER
#include "web/WebResourceManager.h"
#endif
#include <atomic>
#include <array>

namespace ppf42 {

class PPF42AudioProcessorEditor : public juce::AudioProcessorEditor,
                                  public juce::AudioProcessorValueTreeState::Listener,
                                  private juce::Timer
{
public:
    explicit PPF42AudioProcessorEditor(PPF42AudioProcessor&);
    ~PPF42AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

    // APVTS Listener Callback
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // Web Event Handlers (Called from JS)
    void handleParamChangeFromWeb(const juce::var& data);
    void handleLoadPresetFromWeb(const juce::var& data);
    void handleNoteOnFromWeb(const juce::var& data);
    void handleNoteOffFromWeb(const juce::var& data);
    void handleAllNotesOffFromWeb(const juce::var& data);
    void handlePitchBendFromWeb(const juce::var& data);

    void sendParameterUpdateToWeb(const juce::String& paramID, float newValue);
    void syncAllParametersToWeb();

private:
    void timerCallback() override;

#if JUCE_WEB_BROWSER
    static juce::WebBrowserComponent::Options createWebOptions(PPF42AudioProcessorEditor& editor);
#endif

    PPF42AudioProcessor& processorRef;

#if JUCE_WEB_BROWSER
    WebResourceManager resourceManager;
    juce::WebBrowserComponent webComponent;
#endif

    bool initialSyncDone { false };
    bool hwndStylesConfigured { false };
    int  hwndCheckCounter { 0 };
    int  silentFrameCounter { 0 };

    // Lock-free parameter coalescing (60 Hz timer drain)
    std::array<std::atomic<float>, kNumParams> pendingParamValues {};
    std::array<std::atomic<bool>, kNumParams>  paramDirty {};

    void ensureHwndStyles();
    void registerParameterListeners();
    void unregisterParameterListeners();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PPF42AudioProcessorEditor)
};

} // namespace ppf42
