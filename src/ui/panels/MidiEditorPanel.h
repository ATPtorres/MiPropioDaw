#pragma once

#include <JuceHeader.h>
#include "../../core/ProjectState.h"
#include "../../audio/AudioEngine.h"
#include "../../model/TrackList.h"
#include "../../model/PatternLibrary.h"
#include "../widgets/PianoRoll.h"
#include "../widgets/Scales.h"

// Editor MIDI integrado: toolbar (pista, snap, herramientas, escala, zoom),
// teclado, piano roll con playhead y carril de velocidad.
class MidiEditorPanel : public juce::Component,
                        private juce::ValueTree::Listener,
                        private juce::Timer
{
public:
    MidiEditorPanel (ProjectState& state, AudioEngine& engine, TrackList& trackList);
    ~MidiEditorPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class Keyboard : public juce::Component
    {
    public:
        explicit Keyboard (MidiEditorPanel& ownerToUse) : owner (ownerToUse) {}
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;

        std::function<void (int pitch, bool down)> onKey;

    private:
        MidiEditorPanel& owner;
        int lastPitch = -1;
    };

    class VelocityLane : public juce::Component
    {
    public:
        explicit VelocityLane (MidiEditorPanel& ownerToUse) : owner (ownerToUse) {}
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;

    private:
        int noteAt (int x) const;
        void applyVelocity (int noteIndex, int y);

        MidiEditorPanel& owner;
        int dragIndex = -1;
    };

    class SyncedViewport : public juce::Viewport
    {
    public:
        SyncedViewport (MidiEditorPanel& o, int roleToUse) : owner (o), role (roleToUse) {}
        void visibleAreaChanged (const juce::Rectangle<int>& area) override;

    private:
        MidiEditorPanel& owner;
        int role;   // 0 = roll, 1 = teclado, 2 = velocidad
    };

    void timerCallback() override;
    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child) override;
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int index) override;
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& prop) override;

    void refreshTracksCombo();
    void refreshFromPattern();
    void refreshGhosts();
    void applyViewState();
    void syncScroll (int role, const juce::Rectangle<int>& area);
    int targetSlot() const;
    juce::ValueTree targetTrack() const;
    double playheadBeats() const;

    ProjectState& state;
    AudioEngine& engine;
    TrackList& trackList;
    PatternLibrary library;

    juce::ValueTree patternsTree;

    juce::ComboBox trackBox;
    juce::ComboBox snapBox;
    juce::ComboBox scaleBox;
    juce::ComboBox rootBox;
    juce::TextButton drawButton   { "Draw", {} };
    juce::TextButton selectButton { "Select", {} };
    juce::TextButton eraseButton  { "Erase", {} };
    juce::TextButton zoomOutButton { "-", {} };
    juce::TextButton zoomInButton  { "+", {} };
    juce::Label infoLabel;

    Keyboard keyboard { *this };
    VelocityLane velocityLane { *this };
    PianoRoll roll;

    SyncedViewport rollViewport  { *this, 0 };
    SyncedViewport keyViewport   { *this, 1 };
    SyncedViewport velViewport   { *this, 2 };

    double lastPlayhead = -1.0;
    bool ignoreUiCallbacks = false;
    bool syncingScrollNow = false;

    static constexpr int kToolbarH = 30;
    static constexpr int kKeyW = 64;
    static constexpr int kVelH = 76;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiEditorPanel)
};
