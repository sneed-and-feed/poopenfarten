#include "PluginEditor.h"

#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace ppf42 {

#if JUCE_WEB_BROWSER
juce::WebBrowserComponent::Options PPF42AudioProcessorEditor::createWebOptions(PPF42AudioProcessorEditor& editor)
{
#if JUCE_WINDOWS
    // Configure WebView2 Chromium flags for host DAW embedding (FL Studio, Ableton, Reaper, etc.):
    // - Mute browser audio output (C++ DSP engine handles all audio synthesis)
    // - Disable Web MIDI in Chromium (prevents WinMM device contention with DAW MIDI inputs)
    // - Disable background Chromium features that create unneeded threads / network queries
    // - Disable CalculateNativeWinOcclusion to eliminate global SetWinEventHook desktop dragging lag
    _wputenv_s(
        L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS",
        L"--mute-audio "
        L"--disable-audio-output "
        L"--disable-web-midi "
        L"--disable-features=Translate,OptimizationHints,MediaRouter,InterestFeedContentSuggestions,CalculateNativeWinOcclusion"
    );
#endif

    auto options = juce::WebBrowserComponent::Options{}
#if JUCE_WINDOWS
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options(
            juce::WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder(juce::File::getSpecialLocation(juce::File::SpecialLocationType::tempDirectory).getChildFile("PPF42_WebView2"))
                .withBackgroundColour(juce::Colour(0xff0d1117)))
#endif
        .withUserScript("window.__IS_JUCE__ = true;")
        .withNativeIntegrationEnabled()
        .withResourceProvider([&editor](const juce::String& url) {
            return editor.resourceManager.getResource(url);
        })
        .withEventListener("paramChange", [&editor](const juce::var& data) {
            editor.handleParamChangeFromWeb(data);
        })
        .withEventListener("loadPreset", [&editor](const juce::var& data) {
            editor.handleLoadPresetFromWeb(data);
        })
        .withEventListener("noteOn", [&editor](const juce::var& data) {
            editor.handleNoteOnFromWeb(data);
        })
        .withEventListener("noteOff", [&editor](const juce::var& data) {
            editor.handleNoteOffFromWeb(data);
        })
        .withEventListener("allNotesOff", [&editor](const juce::var& data) {
            editor.handleAllNotesOffFromWeb(data);
        })
        .withEventListener("pitchBend", [&editor](const juce::var& data) {
            editor.handlePitchBendFromWeb(data);
        })
        .withEventListener("requestState", [&editor](const juce::var& /*data*/) {
            editor.syncAllParametersToWeb();
        });

    return options;
}
#endif

PPF42AudioProcessorEditor::PPF42AudioProcessorEditor(PPF42AudioProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p)
#if JUCE_WEB_BROWSER
      , webComponent(createWebOptions(*this))
#endif
{
    setOpaque(true);

#if JUCE_WEB_BROWSER
    webComponent.setOpaque(true);
    addAndMakeVisible(webComponent);
#endif

    registerParameterListeners();

    // 16:10 Studio Layout (1280x800 default)
    setSize(1280, 800);
    setResizable(true, true);
    setResizeLimits(960, 600, 2560, 1600);

    // 60 Hz UI Timer for telemetry and coalesced parameters
    startTimerHz(60);

#if JUCE_WEB_BROWSER
    webComponent.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
#endif
}

PPF42AudioProcessorEditor::~PPF42AudioProcessorEditor()
{
    stopTimer();
    unregisterParameterListeners();
}

void PPF42AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0d1117));
}

void PPF42AudioProcessorEditor::resized()
{
#if JUCE_WEB_BROWSER
    webComponent.setBounds(getLocalBounds());
#endif
}

void PPF42AudioProcessorEditor::parentHierarchyChanged()
{
    AudioProcessorEditor::parentHierarchyChanged();
    hwndStylesConfigured = false;
    ensureHwndStyles();
}

void PPF42AudioProcessorEditor::ensureHwndStyles()
{
#if JUCE_WINDOWS && JUCE_WEB_BROWSER
    if (auto* peer = getPeer())
    {
        HWND hwnd = static_cast<HWND>(peer->getNativeHandle());
        if (hwnd == nullptr || !::IsWindow(hwnd))
            return;

        // Apply WS_CLIPCHILDREN | WS_CLIPSIBLINGS to the plugin HWND only
        LONG_PTR style = ::GetWindowLongPtr(hwnd, GWL_STYLE);
        if ((style & (WS_CLIPCHILDREN | WS_CLIPSIBLINGS)) != (WS_CLIPCHILDREN | WS_CLIPSIBLINGS))
        {
            ::SetWindowLongPtr(hwnd, GWL_STYLE, style | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
        }

        // Apply to child WebView2 HWNDs
        int childCount = 0;
        ::EnumChildWindows(hwnd, [](HWND child, LPARAM lParam) -> BOOL {
            if (child == nullptr || !::IsWindow(child))
                return TRUE;
            auto* count = reinterpret_cast<int*>(lParam);
            (*count)++;
            LONG_PTR childStyle = ::GetWindowLongPtr(child, GWL_STYLE);
            if ((childStyle & (WS_CLIPCHILDREN | WS_CLIPSIBLINGS)) != (WS_CLIPCHILDREN | WS_CLIPSIBLINGS))
            {
                ::SetWindowLongPtr(child, GWL_STYLE, childStyle | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&childCount));

        if (childCount > 0)
            hwndStylesConfigured = true;
    }
#endif
}

void PPF42AudioProcessorEditor::registerParameterListeners()
{
    auto& apvts = processorRef.getAPVTS();
    for (const auto& meta : kParamRegistry)
    {
        apvts.addParameterListener(meta.apvtsId, this);
    }
}

void PPF42AudioProcessorEditor::unregisterParameterListeners()
{
    auto& apvts = processorRef.getAPVTS();
    for (const auto& meta : kParamRegistry)
    {
        apvts.removeParameterListener(meta.apvtsId, this);
    }
}

void PPF42AudioProcessorEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Coalesce dirty updates into atomic array (zero allocations / zero lock contention)
    for (size_t i = 0; i < kNumParams; ++i)
    {
        if (parameterID == kParamRegistry[i].apvtsId)
        {
            pendingParamValues[i].store(newValue, std::memory_order_relaxed);
            paramDirty[i].store(true, std::memory_order_relaxed);
            break;
        }
    }
}

void PPF42AudioProcessorEditor::handleParamChangeFromWeb(const juce::var& data)
{
    if (!data.isObject())
        return;

    const juce::String id = data.getProperty("id", "").toString();
    const float val = static_cast<float>(data.getProperty("value", 0.0));

    if (auto* param = processorRef.getAPVTS().getParameter(id))
    {
        param->setValueNotifyingHost(param->convertTo0to1(val));
    }
}

void PPF42AudioProcessorEditor::handleLoadPresetFromWeb(const juce::var& data)
{
    int index = 0;
    if (data.isObject())
        index = static_cast<int>(data.getProperty("index", 0));
    else if (data.isInt() || data.isInt64() || data.isDouble())
        index = static_cast<int>(data);

    processorRef.setCurrentProgram(index);
    syncAllParametersToWeb();
}

void PPF42AudioProcessorEditor::handleNoteOnFromWeb(const juce::var& data)
{
    if (data.isObject())
    {
        const int note = static_cast<int>(data.getProperty("note", 60));
        const float vel = static_cast<float>(data.getProperty("velocity", 0.8));
        processorRef.pushUINoteOn(note, vel);
    }
}

void PPF42AudioProcessorEditor::handleNoteOffFromWeb(const juce::var& data)
{
    if (data.isObject())
    {
        const int note = static_cast<int>(data.getProperty("note", 60));
        const float vel = static_cast<float>(data.getProperty("velocity", 0.0));
        processorRef.pushUINoteOff(note, vel);
    }
}

void PPF42AudioProcessorEditor::handleAllNotesOffFromWeb(const juce::var& /*data*/)
{
    processorRef.pushUIAllNotesOff();
}

void PPF42AudioProcessorEditor::handlePitchBendFromWeb(const juce::var& data)
{
    if (data.isObject())
    {
        const float cents = static_cast<float>(data.getProperty("cents", 0.0));
        processorRef.pushUIPitchBend(cents);
    }
}

void PPF42AudioProcessorEditor::sendParameterUpdateToWeb(const juce::String& paramID, float newValue)
{
#if JUCE_WEB_BROWSER
    auto* obj = new juce::DynamicObject();
    obj->setProperty("id", paramID);
    obj->setProperty("value", newValue);
    webComponent.emitEventIfBrowserIsVisible("paramUpdate", juce::var(obj));
#else
    juce::ignoreUnused(paramID, newValue);
#endif
}

void PPF42AudioProcessorEditor::syncAllParametersToWeb()
{
#if JUCE_WEB_BROWSER
    auto& apvts = processorRef.getAPVTS();
    auto* allParamsObj = new juce::DynamicObject();
    auto* paramsList = new juce::DynamicObject();

    for (const auto& meta : kParamRegistry)
    {
        if (auto* raw = apvts.getRawParameterValue(meta.apvtsId))
        {
            paramsList->setProperty(meta.apvtsId, raw->load(std::memory_order_relaxed));
        }
    }

    allParamsObj->setProperty("params", juce::var(paramsList));
    allParamsObj->setProperty("currentPreset", processorRef.getCurrentProgram());
    allParamsObj->setProperty("presetName", processorRef.getProgramName(processorRef.getCurrentProgram()));
    webComponent.emitEventIfBrowserIsVisible("stateSync", juce::var(allParamsObj));
#endif
}

void PPF42AudioProcessorEditor::timerCallback()
{
#if JUCE_WEB_BROWSER
    if (!webComponent.isVisible())
        return;

    // Check Win32 HWND clipping styles periodically
    if (!hwndStylesConfigured || ++hwndCheckCounter >= 60)
    {
        hwndCheckCounter = 0;
        ensureHwndStyles();
    }

    // Initial state sync when WebView2 becomes ready
    if (!initialSyncDone)
    {
        syncAllParametersToWeb();
        initialSyncDone = true;
    }

    // 1. Flush coalesced dirty APVTS parameters to WebView
    for (size_t i = 0; i < kNumParams; ++i)
    {
        if (paramDirty[i].exchange(false, std::memory_order_relaxed))
        {
            const float val = pendingParamValues[i].load(std::memory_order_relaxed);
            sendParameterUpdateToWeb(kParamRegistry[i].apvtsId, val);
        }
    }

    // 2. Stream real-time visualizer telemetry & oscilloscope waveform
    VisualizerFrame frame {};
    if (processorRef.popVisualizerFrame(frame))
    {
        // Silence detection & CPU conservation:
        const bool isSilent = (frame.outputRmsL < 0.0001f && frame.outputRmsR < 0.0001f);
        if (isSilent)
        {
            if (++silentFrameCounter > 6 && (silentFrameCounter % 15 != 0))
            {
                return; // Throttle to ~4 Hz during prolonged silence
            }
        }
        else
        {
            silentFrameCounter = 0;
        }

        // Convert 128 scope samples to Base64 uint8 bytes
        uint8_t bytes[kTelemetryScopeSamples];
        for (size_t i = 0; i < kTelemetryScopeSamples; ++i)
        {
            const float s = std::clamp(frame.scopeSamples[i], -1.0f, 1.0f);
            bytes[i] = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(128.0f + s * 127.0f)), 0, 255));
        }

        auto* obj = new juce::DynamicObject();
        obj->setProperty("waveform", juce::Base64::toBase64(bytes, kTelemetryScopeSamples));
        obj->setProperty("pressure", frame.colonicPressure);
        obj->setProperty("velocity", frame.airflowVelocity);
        obj->setProperty("aperture", frame.instantaneousAperture);
        obj->setProperty("bubbleDensity", frame.bubbleActivity);
        obj->setProperty("dropletPops", frame.dropletPops);
        obj->setProperty("cleftEnergy", frame.cleftResonanceEnergy);
        obj->setProperty("rmsL", frame.outputRmsL);
        obj->setProperty("rmsR", frame.outputRmsR);

        webComponent.emitEventIfBrowserIsVisible("visualizerFrame", juce::var(obj));
    }
#endif
}

} // namespace ppf42
