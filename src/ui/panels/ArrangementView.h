#pragma once

#include <JuceHeader.h>
#include "../../core/ProjectState.h"
#include "../../model/TrackList.h"
#include "../../audio/AudioEngine.h"
#include "../widgets/TrackHeader.h"

// Vista de arreglo: cabeceras de pista (izquierda) + timeline con rejilla,
// barra de transporte integrada y cabezal de reproducción.
class ArrangementView : public juce::Component,
                        private juce::ValueTree::Listener,
                        private juce::MouseListener,
                        private juce::Timer
{
public:
    ArrangementView (ProjectState& state, AudioEngine& engine, TrackList& trackList);
    ~ArrangementView() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    // ---------------------------------------------------------------
    class TimelineContent : public juce::Component
    {
    public:
        TimelineContent (ArrangementView& ownerToUse) : owner (ownerToUse) {}
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;

    private:
        ArrangementView& owner;
    };

    class TimelineViewport : public juce::Viewport
    {
    public:
        explicit TimelineViewport (ArrangementView& ownerToUse) : owner (ownerToUse) {}
        void visibleAreaChanged (const juce::Rectangle<int>& newVisibleArea) override;

    private:
        ArrangementView& owner;
    };

    void timerCallback() override;
    void rebuildHeaders();
    void layoutContents();
    void addTrack (const juce::String& type);

    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child) override;
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int index) override;
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& prop) override;

    int timeSigNum() const;
    int timelineWidth() const;
    int contentHeight() const;

    ProjectState& state;
    AudioEngine& engine;
    TrackList& trackList;

    juce::ValueTree tracksTree;
    juce::ValueTree transportTree;

    // toolbar
    juce::Label tracksLabel { {}, "TRACKS" };
    juce::TextButton addAudioButton { "+ Audio", {} };
    juce::TextButton addMidiButton  { "+ MIDI", {} };
    juce::Label hintLabel { {}, "Click en el nombre para editar · Ch = entrada de audio" };

    // cabeceras
    juce::Viewport headerViewport;
    juce::Component headerColumn;
    std::vector<std::unique_ptr<TrackHeader>> headers;

    // timeline
    TimelineViewport timelineViewport { *this };
    TimelineContent timelineContent { *this };

    static constexpr int kToolbarH = 32;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangementView)
};
