#pragma once

#include <JuceHeader.h>
#include "../core/ProjectState.h"
#include "../model/TrackList.h"
#include "../audio/AudioEngine.h"
#include "MainLayout.h"

// Ventana principal única (sin ventanas flotantes).
class MainWindow : public juce::DocumentWindow,
                   private juce::KeyListener
{
public:
    MainWindow (ProjectState& state, AudioEngine& engine, TrackList& trackList);
    ~MainWindow() override;

    void closeButtonPressed() override;

private:
    // KeyListener
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

    void saveProject (bool forceChooseFile);
    void openProject ();
    void showAudioSettings ();
    void refreshTitle();

    ProjectState& state;
    AudioEngine& engine;
    MainLayout* layout = nullptr;   // propiedad de DocumentWindow (setContentOwned)

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};
