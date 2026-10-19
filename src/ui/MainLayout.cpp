#include "MainLayout.h"
#include "Theme.h"

MainLayout::MainLayout (ProjectState& stateToUse, AudioEngine& engineToUse, TrackList& trackListToUse)
    : state (stateToUse),
      engine (engineToUse),
      trackList (trackListToUse),
      transportBar (stateToUse, engineToUse),
      arrangementView (stateToUse, engineToUse, trackListToUse),
      sessionView (stateToUse, engineToUse, trackListToUse),
      mixerView (stateToUse, engineToUse, trackListToUse),
      rightSplit (false, 0.58f, &viewHost, &bottomTabs, 180.0f, 150.0f),
      mainSplit (true, 0.15f, &leftTabs, &rightSplit, 190.0f, 420.0f)
{
    addAndMakeVisible (transportBar);
    addAndMakeVisible (mainSplit);

    transportBar.onViewModeChanged = [this] (const juce::String& mode) { setViewMode (mode); };

    leftTabs.setColour (juce::TabbedComponent::backgroundColourId, Theme::panel());
    leftTabs.addTab ("Browser", Theme::panel(), new BrowserPanel(), true);
    leftTabs.addTab ("Patterns", Theme::panel(), new PatternSidebar (stateToUse, engineToUse), true);
    leftTabs.setCurrentTabIndex (0);

    bottomTabs.setColour (juce::TabbedComponent::backgroundColourId, Theme::panel());
    bottomTabs.addTab ("Mixer", Theme::panel(), &mixerView, false);
    bottomTabs.addTab ("MIDI Editor", Theme::panel(), new MidiEditorPanel (stateToUse, engineToUse, trackListToUse), true);
    bottomTabs.setCurrentTabIndex (0);

    setViewMode (state.root.getProperty (IDs::viewMode, "arrangement").toString());
}

void MainLayout::setViewMode (const juce::String& mode)
{
    state.root.setProperty (IDs::viewMode, mode, nullptr);

    viewHost.setView (mode == "session" ? static_cast<juce::Component*> (&sessionView)
                                        : static_cast<juce::Component*> (&arrangementView));
}

MainLayout::~MainLayout()
{
    leftTabs.clearTabs();
    bottomTabs.clearTabs();
}

void MainLayout::paint (juce::Graphics& g)
{
    g.fillAll (Theme::window());
}

void MainLayout::resized()
{
    auto r = getLocalBounds();
    transportBar.setBounds (r.removeFromTop (Theme::transportHeight));
    mainSplit.setBounds (r);
}
