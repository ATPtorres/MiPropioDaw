#pragma once

#include <JuceHeader.h>
#include "../../core/ProjectState.h"
#include "../../model/TrackList.h"
#include "../../model/PatternLibrary.h"
#include "../../audio/AudioEngine.h"
#include "../widgets/TrackHeader.h"

// Vista de arreglo: cabeceras de pista (izquierda) + timeline con rejilla,
// clips MIDI arrastrables y cabezal de reproducción.
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
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;

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

    struct ClipHit
    {
        juce::ValueTree track, clip;
        int trackIndex = -1;
        bool rightEdge = false;
        bool isValid() const { return clip.isValid(); }
    };

    ClipHit hitClip (int x, int y) const;
    int trackIndexAtY (int y) const;
    static double snapBeat (double beats);

    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child) override;
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int index) override;
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& prop) override;

    int timeSigNum() const;
    int timelineWidth() const;
    int contentHeight() const;

    ProjectState& state;
    AudioEngine& engine;
    TrackList& trackList;
    PatternLibrary library;

    juce::ValueTree tracksTree;
    juce::ValueTree transportTree;

    enum class ClipDrag { none, move, resize };
    ClipDrag clipDrag = ClipDrag::none;
    juce::ValueTree dragClip, dragClipTrack;
    int dragDownX = 0;
    double dragOrigStart = 0.0, dragOrigLen = 0.0;

    // toolbar
    juce::Label tracksLabel { {}, "TRACKS" };
    juce::TextButton addAudioButton { "+ Audio", {} };
    juce::TextButton addMidiButton  { "+ MIDI", {} };
    juce::Label hintLabel { {}, "Click en MIDI = clip del patrón activo · clic derecho = borrar" };

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
