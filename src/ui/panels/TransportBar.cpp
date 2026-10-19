#include "TransportBar.h"
#include "../Theme.h"
#include "../../core/IDs.h"

#if JUCE_WINDOWS
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <psapi.h>
 #pragma comment(lib, "psapi.lib")
#endif

namespace
{
    size_t processRamBytes()
    {
       #if JUCE_WINDOWS
        PROCESS_MEMORY_COUNTERS pmc {};
        if (GetProcessMemoryInfo (GetCurrentProcess(), &pmc, sizeof (pmc)))
            return (size_t) pmc.WorkingSetSize;
       #endif
        return 0;
    }
}

TransportBar::TransportBar (ProjectState& stateToUse, AudioEngine& engineToUse)
    : state (stateToUse), engine (engineToUse), transport (state.transport())
{
    playButton.setColour (juce::TextButton::buttonColourId, Theme::accent());
    playButton.onClick = [this] { engine.togglePlay(); };
    addAndMakeVisible (playButton);

    stopButton.onClick = [this] { engine.stop(); };
    addAndMakeVisible (stopButton);

    recordButton.setClickingTogglesState (true);
    recordButton.setColour (juce::TextButton::buttonColourId, Theme::danger());
    recordButton.onClick = [this]
    {
        if (! ignoreUiCallbacks)
            state.setProperty (transport, IDs::record, recordButton.getToggleState(), "Record arm");
    };
    addAndMakeVisible (recordButton);

    positionLabel.setFont (Theme::font (17.0f, true));
    positionLabel.setJustificationType (juce::Justification::centred);
    positionLabel.setColour (juce::Label::textColourId, Theme::accent());
    positionLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (positionLabel);

    bpmCaption.setText ("BPM", juce::dontSendNotification);
    bpmCaption.setFont (Theme::font (10.0f));
    bpmCaption.setColour (juce::Label::textColourId, Theme::textDim());
    bpmCaption.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (bpmCaption);

    bpmLabel.setFont (Theme::font (15.0f, true));
    bpmLabel.setJustificationType (juce::Justification::centred);
    bpmLabel.setEditable (true, true, true);
    bpmLabel.onTextChange = [this]
    {
        if (ignoreUiCallbacks)
            return;

        const auto value = juce::jlimit (20.0, 999.0, bpmLabel.getText().getDoubleValue());
        state.setProperty (transport, IDs::bpm, value, "Set tempo");
        refreshFromTree();
    };
    addAndMakeVisible (bpmLabel);

    sigLabel.setFont (Theme::font (13.0f));
    sigLabel.setJustificationType (juce::Justification::centred);
    sigLabel.setEditable (true, true, true);
    sigLabel.onTextChange = [this]
    {
        if (ignoreUiCallbacks)
            return;

        auto parts = juce::StringArray::fromTokens (sigLabel.getText().trim(), "/", "");
        parts.trim();

        const auto num = juce::jlimit (1, 32, parts[0].getIntValue());
        const auto den = parts.size() > 1 ? juce::jlimit (1, 32, parts[1].getIntValue()) : 4;

        state.setProperty (transport, IDs::timeSigNum, num, "Set time signature");
        state.setProperty (transport, IDs::timeSigDen, den, "Set time signature");
        refreshFromTree();
    };
    addAndMakeVisible (sigLabel);

    metroButton.setClickingTogglesState (true);
    metroButton.onClick = [this]
    {
        if (! ignoreUiCallbacks)
            state.setProperty (transport, IDs::metronome, metroButton.getToggleState(), "Metronome");
    };
    addAndMakeVisible (metroButton);

    loopButton.setClickingTogglesState (true);
    loopButton.onClick = [this]
    {
        if (! ignoreUiCallbacks)
            state.setProperty (transport, IDs::loop, loopButton.getToggleState(), "Loop");
    };
    addAndMakeVisible (loopButton);

    addAndMakeVisible (cpuBar);
    addAndMakeVisible (ramLabel);
    addAndMakeVisible (latLabel);

    saveButton.onClick = [this] { if (onSave) onSave(); };
    addAndMakeVisible (saveButton);

    openButton.onClick = [this] { if (onOpen) onOpen(); };
    addAndMakeVisible (openButton);

    settingsButton.onClick = [this] { if (onAudioSettings) onAudioSettings(); };
    addAndMakeVisible (settingsButton);

    arrangementButton.setColour (juce::TextButton::buttonColourId, Theme::panel());
    arrangementButton.setColour (juce::TextButton::buttonOnColourId, Theme::panel());
    arrangementButton.setColour (juce::TextButton::textColourOffId, Theme::textDim());
    arrangementButton.setColour (juce::TextButton::textColourOnId, Theme::text());
    arrangementButton.onClick = [this]
    {
        if (onViewModeChanged)
            onViewModeChanged ("arrangement");
    };
    addAndMakeVisible (arrangementButton);

    sessionButton.setColour (juce::TextButton::buttonColourId, Theme::panel());
    sessionButton.setColour (juce::TextButton::buttonOnColourId, Theme::panel());
    sessionButton.setColour (juce::TextButton::textColourOffId, Theme::textDim());
    sessionButton.setColour (juce::TextButton::textColourOnId, Theme::text());
    sessionButton.onClick = [this]
    {
        if (onViewModeChanged)
            onViewModeChanged ("session");
    };
    addAndMakeVisible (sessionButton);

    state.root.addListener (this);
    refreshFromTree();
    refreshMode();

    startTimerHz (20);
}

TransportBar::~TransportBar()
{
    state.root.removeListener (this);
    stopTimer();
}

void TransportBar::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel());

    g.setColour (Theme::border());
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    auto* active = sessionButton.getToggleState() ? &sessionButton : &arrangementButton;

    if (active->isVisible())
    {
        g.setColour (Theme::accent());
        g.fillRect (active->getX() + 6, active->getBottom() + 1,
                    active->getWidth() - 12, 2);
    }
}

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    const int h = r.getHeight();

    // --- cluster izquierdo: botones de transporte ---------------------
    auto left = r.removeFromLeft (3 * 42 + 2 * 6);
    playButton.setBounds   (left.removeFromLeft (42).reduced (0, (h - 34) / 2));
    left.removeFromLeft (6);
    stopButton.setBounds   (left.removeFromLeft (42).reduced (0, (h - 34) / 2));
    left.removeFromLeft (6);
    recordButton.setBounds (left.removeFromLeft (42).reduced (0, (h - 34) / 2));

    r.removeFromLeft (16);

    // --- cluster derecho (derecha a izquierda) ------------------------
    auto right = r;
    int x = right.getRight();

    auto placeRight = [&x, &right] (juce::Component& c, int w, int gap)
    {
        x -= gap;
        c.setBounds (x - w, right.getY(), w, right.getHeight());
        x -= w;
        c.setVisible (true);
    };

    const bool showMeta   = getWidth() >= 1180;
    const bool showStatus = getWidth() >= 1080;

    settingsButton.setVisible (showStatus);
    openButton.setVisible (showStatus);
    saveButton.setVisible (showStatus);

    if (showStatus)
    {
        placeRight (settingsButton, 72, 0);
        placeRight (openButton, 50, 6);
        placeRight (saveButton, 48, 6);
    }

    if (showMeta)
    {
        latLabel.setVisible (true);
        ramLabel.setVisible (true);
        placeRight (latLabel, 74, 12);
        placeRight (ramLabel, 84, 4);
    }
    else
    {
        latLabel.setVisible (false);
        ramLabel.setVisible (false);
    }

    cpuBar.setVisible (true);
    placeRight (cpuBar, 76, showMeta ? 12 : 0);

    sessionButton.setVisible (getWidth() >= 1320);
    arrangementButton.setVisible (getWidth() >= 1320);

    if (sessionButton.isVisible())
    {
        placeRight (sessionButton, 84, 14);
        placeRight (arrangementButton, 100, 4);
    }

    const int rightEnd = x;

    // --- cluster central (centrado entre izquierda y derecha) ---------
    int middleW = 0;
    middleW += 108;          // posición
    middleW += 34;           // BPM caption
    middleW += 62;           // BPM valor
    middleW += 50;           // compás
    middleW += 66;           // metrónomo
    middleW += 58;           // loop
    middleW += 5 * 8;        // gaps

    const int leftEnd = r.getX() + 138 + 16;
    int middleX = leftEnd + juce::jmax (0, (rightEnd - leftEnd - middleW) / 2);

    auto mid = r.withX (middleX).withWidth (middleW);
    mid.reduce (0, (h - 34) / 2);

    auto next = [&mid] (int w, int gap) {
        mid.removeFromLeft (gap);
        return mid.removeFromLeft (w);
    };

    positionLabel.setBounds (next (108, 0));
    bpmCaption.setBounds    (next (34, 10));
    bpmLabel.setBounds      (next (62, 2));
    sigLabel.setBounds      (next (50, 8));
    metroButton.setBounds   (next (66, 10));
    loopButton.setBounds    (next (58, 8));
}

void TransportBar::timerCallback()
{
    positionLabel.setText (engine.positionText(), juce::dontSendNotification);
    playButton.setToggleState (engine.isPlaying(), juce::dontSendNotification);

    cpuBar.setPercent (engine.getCpuPercent());

   #if JUCE_WINDOWS
    const auto ram = processRamBytes();
    ramLabel.setText (juce::String (ram / (1024 * 1024)) + " MB", juce::dontSendNotification);
   #endif

    latLabel.setText (juce::String (engine.getLatencyMs(), 1) + " ms", juce::dontSendNotification);
}

void TransportBar::refreshFromTree()
{
    ignoreUiCallbacks = true;

    if (! bpmLabel.hasKeyboardFocus (true))
    {
        const double value = (double) transport.getProperty (IDs::bpm, 140.0);
        bpmLabel.setText (value == std::floor (value) ? juce::String ((int) value)
                                                      : juce::String (value, 2),
                          juce::dontSendNotification);
    }

    if (! sigLabel.hasKeyboardFocus (true))
    {
        sigLabel.setText (juce::String ((int) transport.getProperty (IDs::timeSigNum, 4)) + "/"
                            + juce::String ((int) transport.getProperty (IDs::timeSigDen, 4)),
                          juce::dontSendNotification);
    }

    metroButton.setToggleState ((bool) transport.getProperty (IDs::metronome, false), juce::dontSendNotification);
    loopButton.setToggleState  ((bool) transport.getProperty (IDs::loop, false), juce::dontSendNotification);
    recordButton.setToggleState ((bool) transport.getProperty (IDs::record, false), juce::dontSendNotification);

    ignoreUiCallbacks = false;
}

void TransportBar::refreshMode()
{
    const bool session = state.root.getProperty (IDs::viewMode, "arrangement").toString() == "session";

    arrangementButton.setToggleState (! session, juce::dontSendNotification);
    sessionButton.setToggleState (session, juce::dontSendNotification);
    repaint();
}

void TransportBar::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& prop)
{
    if (tree == state.root)
    {
        if (prop == juce::Identifier (IDs::viewMode))
            refreshMode();
        return;
    }

    if (tree == transport)
        refreshFromTree();
}
