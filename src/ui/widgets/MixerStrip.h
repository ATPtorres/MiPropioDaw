#pragma once

#include <JuceHeader.h>
#include "../../core/ProjectState.h"
#include "../../audio/AudioEngine.h"
#include "VUMeter.h"
#include "ProChannelPanel.h"
#include "SpectrumDisplay.h"

// Tira vertical de canal del mixer: nombre, VU analógico, ProChannel, fader, pan, M/S.
class MixerStrip : public juce::Component,
                   private juce::ValueTree::Listener
{
public:
    // Pista normal (con árbol ValueTree)
    MixerStrip (juce::ValueTree trackTree, ProjectState& state, AudioEngine& engine);
    // Master (sin árbol; usa mixer.masterGain)
    MixerStrip (juce::ValueTree mixerTree, bool isMasterStrip, ProjectState& state, AudioEngine& engine);
    ~MixerStrip() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void refreshMeter();   // llamado por el timer del MixerView (también integra VU)
    void tickVU (double dt); // integra la física de la aguja VU

private:
    void commonSetup();
    void refreshFromTree();
    void valueTreePropertyChanged (juce::ValueTree& treeWhosePropertyChanged,
                                   const juce::Identifier& property) override;

    juce::ValueTree tree;       // track o mixer
    juce::ValueTree trackTree;  // sólo pista (nulo en master)
    ProjectState& state;
    AudioEngine& engine;
    int slotIndex = -1;
    bool master = false;

    juce::Label nameLabel;
    juce::Label slotLabel;
    juce::Slider fader;
    juce::Slider panKnob;
    juce::TextButton muteButton { "M", {} };
    juce::TextButton soloButton { "S", {} };

    std::unique_ptr<VUMeter> vuMeter;
    std::unique_ptr<ProChannelPanel> proChannel;   // sólo en tiras normales
    SpectrumDisplay spectrum;                      // sólo en master

    bool ignoreUiCallbacks = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerStrip)
};
