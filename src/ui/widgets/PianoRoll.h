#pragma once

#include <JuceHeader.h>
#include "../Theme.h"
#include "../../core/ProjectState.h"
#include "../../model/PatternLibrary.h"
#include "Scales.h"

// Piano roll: rejilla de notas con herramientas dibujar/seleccionar/borrar,
// resaltado de escala y notas fantasma de otras pistas.
class PianoRoll : public juce::Component,
                  private juce::ValueTree::Listener
{
public:
    enum class Tool { draw, select, erase };

    struct Ghost
    {
        double start = 0.0, length = 0.0;
        int pitch = 60;
    };

    static constexpr int kRowHeight = 14;

    PianoRoll (ProjectState& state, PatternLibrary& library);
    ~PianoRoll() override;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    bool keyPressed (const juce::KeyPress& key) override;

    void setPattern (const juce::ValueTree& patternTree);
    void setGhosts (std::vector<Ghost> newGhosts);
    void setTrackColour (juce::Colour c);
    void setTool (Tool t);
    void setSnapBeats (double beats);
    void setScale (int rootPitchClass, const juce::Array<int>& intervals);
    void setZoom (float newZoom);
    void setPlayhead (double beats);
    float getZoom() const noexcept { return zoom; }
    juce::Colour getTrackColour() const noexcept { return trackColour; }

    double contentBeats() const;
    int contentWidth() const;
    int contentHeight() const;
    double pxPerBeat() const noexcept { return Theme::pxPerBeat * (double) zoom; }

    int yForPitch (int pitch) const noexcept { return (127 - pitch) * kRowHeight; }
    int pitchForY (int y) const noexcept { return 127 - y / kRowHeight; }

    std::function<void (int pitch, bool down)> onPreviewKey;
    std::function<void()> onContentChanged;
    juce::ValueTree selectedNote;

private:
    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child) override;
    void valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int index) override;
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& prop) override;

    double snap (double beats) const;
    juce::ValueTree hitNote (int x, int y) const;
    int velocityForY (int y) const;
    void drawNote (juce::Graphics& g, double start, double len, int pitch,
                   double vel, juce::Colour colour, bool ghost, bool selected);

    ProjectState& state;
    PatternLibrary& library;
    juce::ValueTree pattern;
    std::vector<Ghost> ghosts;
    juce::Colour trackColour { 0xff4fc3f7 };
    Tool tool = Tool::draw;
    double snapBeats = 0.25;
    int scaleRoot = 0;
    juce::Array<int> scaleIntervals;
    float zoom = 1.0f;
    double playheadBeats = -1.0;

    enum class Drag { none, create, move, resize };
    Drag dragMode = Drag::none;
    juce::ValueTree dragNote;
    double dragOrigStart = 0.0, dragOrigLen = 0.0;
    int dragOrigPitch = 60;
    int dragDownX = 0, dragDownY = 0;
    bool previewing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRoll)
};
