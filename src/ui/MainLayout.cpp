#include "MainLayout.h"
#include "Theme.h"

MainLayout::MainLayout (ProjectState& stateToUse, AudioEngine& engineToUse, TrackList& trackList)
    : state (stateToUse),
      engine (engineToUse),
      transportBar (stateToUse, engineToUse),
      arrangementView (stateToUse, engineToUse, trackList),
      mixerView (stateToUse, engineToUse, trackList),
      rightSplit (false, 0.58f, &arrangementView, &bottomTabs, 180.0f, 150.0f),
      mainSplit (true, 0.15f, &leftTabs, &rightSplit, 190.0f, 420.0f)
{
    addAndMakeVisible (transportBar);
    addAndMakeVisible (mainSplit);

    leftTabs.setColour (juce::TabbedComponent::backgroundColourId, Theme::panel());
    leftTabs.addTab ("Browser", Theme::panel(), new BrowserPanel(), true);
    leftTabs.addTab ("Patterns", Theme::panel(), new PatternSidebar (stateToUse), true);
    leftTabs.setCurrentTabIndex (0);

    bottomTabs.setColour (juce::TabbedComponent::backgroundColourId, Theme::panel());
    bottomTabs.addTab ("Mixer", Theme::panel(), &mixerView, false);
    bottomTabs.addTab ("MIDI Editor", Theme::panel(), new MidiEditorPanel(), true);
    bottomTabs.setCurrentTabIndex (0);
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
