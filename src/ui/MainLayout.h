#pragma once

#include <JuceHeader.h>
#include "../core/ProjectState.h"
#include "../model/TrackList.h"
#include "../audio/AudioEngine.h"
#include "panels/TransportBar.h"
#include "panels/BrowserPanel.h"
#include "panels/PatternSidebar.h"
#include "panels/ArrangementView.h"
#include "panels/MixerView.h"
#include "panels/MidiEditorPanel.h"
#include "SplitPane.h"

// Layout principal "todo en uno" — una sola ventana con paneles acoplables:
//
//   ┌──────────────── TransportBar ────────────────┐
//   │ Browser/Patterns │ Arrangement               │
//   │ (SplitPane horiz) ├──────────────────────────│
//   │                  │ Mixer / MIDI (SplitPane v)│
//   └──────────────────────────────────────────────┘
class MainLayout : public juce::Component
{
public:
    MainLayout (ProjectState& state, AudioEngine& engine, TrackList& trackList);
    ~MainLayout() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    TransportBar& getTransportBar() noexcept { return transportBar; }

private:
    ProjectState& state;
    AudioEngine& engine;

    TransportBar transportBar;
    ArrangementView arrangementView;
    MixerView mixerView;

    juce::TabbedComponent leftTabs  { juce::TabbedButtonBar::TabsAtTop };
    juce::TabbedComponent bottomTabs{ juce::TabbedButtonBar::TabsAtTop };

    SplitPane rightSplit;
    SplitPane mainSplit;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainLayout)
};
