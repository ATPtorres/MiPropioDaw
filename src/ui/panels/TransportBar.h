#pragma once

#include <JuceHeader.h>
#include "../../core/ProjectState.h"
#include "../../audio/AudioEngine.h"
#include "../widgets/TransportWidgets.h"

// Panel superior: transporte global, BPM, compás, metrónomo, CPU/RAM,
// atajos de archivo y selector de modo.
class TransportBar : public juce::Component,
                     private juce::Timer,
                     private juce::ValueTree::Listener
{
public:
    TransportBar (ProjectState& state, AudioEngine& engine);
    ~TransportBar() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    std::function<void()> onSave;
    std::function<void()> onOpen;
    std::function<void()> onAudioSettings;

private:
    void timerCallback() override;
    void refreshFromTree();

    void valueTreePropertyChanged (juce::ValueTree& treeWhosePropertyChanged,
                                   const juce::Identifier& property) override;

    ProjectState& state;
    AudioEngine& engine;
    juce::ValueTree transport;

    TransportButton playButton  { TransportButton::play,   "Play" };
    TransportButton stopButton  { TransportButton::stop,   "Stop" };
    TransportButton recordButton{ TransportButton::record, "Record" };

    juce::Label positionLabel;
    juce::Label bpmCaption;
    juce::Label bpmLabel;
    juce::Label sigLabel;
    juce::TextButton metroButton { "Metro", {} };
    juce::TextButton loopButton  { "Loop", {} };

    CpuBar cpuBar;
    juce::Label ramLabel;
    juce::Label latLabel;

    juce::TextButton saveButton { "Save", {} };
    juce::TextButton openButton { "Open", {} };
    juce::TextButton settingsButton { "Audio...", {} };

    juce::Label arrangementMode { {}, "ARRANGEMENT" };
    juce::Label sessionMode     { {}, "SESSION (Fase 4)" };

    bool ignoreUiCallbacks = false;
    bool editingBpm = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportBar)
};
