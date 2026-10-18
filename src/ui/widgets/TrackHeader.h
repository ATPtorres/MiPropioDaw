#pragma once

#include <JuceHeader.h>
#include "../../core/ProjectState.h"
#include "../../model/TrackList.h"
#include "../../audio/AudioEngine.h"

// Cabecera de pista en la vista de arreglo:
// fila 1: color + nombre (editable) + eliminar
// fila 2: entrada de audio + Mute + Solo
class TrackHeader : public juce::Component,
                    private juce::ValueTree::Listener
{
public:
    TrackHeader (juce::ValueTree trackTree, ProjectState& state, TrackList& list, AudioEngine& engine);
    ~TrackHeader() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void refreshFromTree();
    void showColourMenu();
    void rebuildInputCombo();

    juce::ValueTree tree;
    ProjectState& state;
    TrackList& trackList;
    AudioEngine& engine;

    struct ColourSwatch : juce::Component
    {
        TrackHeader& owner;
        explicit ColourSwatch (TrackHeader& o) : owner (o) {}
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent&) override;
    };

    ColourSwatch swatch { *this };
    juce::Label nameLabel;
    juce::ComboBox inputBox;
    juce::TextButton muteButton { "M", {} };
    juce::TextButton soloButton { "S", {} };
    juce::TextButton deleteButton { juce::String(), {} };

    bool ignoreUiCallbacks = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackHeader)
};
